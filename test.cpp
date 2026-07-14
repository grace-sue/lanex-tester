#include "test.h"
#include "constants.h"
#include <unistd.h>
#include <thread>
#include <vector>
#include <future>
#include <chrono>
#include <string>
#include <ctime>
#include <atomic>

namespace LANEXTest {

    static const int PORT_START = 5201;

    static std::string timestampNow() {
        time_t raw;
        time(&raw);
        struct tm *t = localtime(&raw);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
        return std::string(buf);
    }

    // Runs one iperf measurement (local server + remote client) synchronously and
    // returns its result. Only touches this pair's slots in `td`, so it is safe to
    // run several pairs concurrently (each owns its index).
    static IperfExecutor::IperfResult runOneMeasurement(
            testData &td, ServerConfigurationLoader::ServerConfiguration *sc,
            int pair, int port, int duration, bool reversed, int cap, bool bidir) {

        IperfExecutor::stopLocalIperf3Server(port); // clear any lingering server on this port
        td.currentRxRate[pair] = -1; td.currentTxRate[pair] = -1;
        td.averageRxRate[pair] = -1; td.averageTxRate[pair] = -1;

        std::thread server(IperfExecutor::startLocalIperf3Server, port, duration + 15);
        usleep(300 * 1000); // give the server a moment to bind before the client connects

        IperfExecutor::IperfResult res = IperfExecutor::startRemoteIperf3Client(
            duration, &td, pair, sc->clientIps[pair], sc->serverIps[pair], port, reversed, cap, bidir);

        // The client has returned (finished or dropped). Kill the local server now so
        // join() returns promptly — otherwise a dropped connection leaves the --one-off
        // server stuck on the dead socket until its own timeout, stalling the measurement.
        IperfExecutor::stopLocalIperf3Server(port);
        server.join();
        return res;
    }

    // Runs a measurement, retrying ONLY when it couldn't start (setup error) and the
    // operator has not asked to stop. A genuine connection drop is returned immediately.
    static IperfExecutor::IperfResult measureWithRetry(
            testData &td, ServerConfigurationLoader::ServerConfiguration *sc,
            int pair, int port, int duration, bool reversed, int cap, bool bidir,
            int retries, std::atomic<bool> &stop) {

        IperfExecutor::IperfResult res =
            runOneMeasurement(td, sc, pair, port, duration, reversed, cap, bidir);
        int attempt = 0;
        while(res == IperfExecutor::IPERF_RESULT_SETUP_ERROR && attempt < retries && !stop) {
            attempt++;
            td.retryAttempt[pair].store(attempt);   // published to the UI pump for the amber cell
            res = runOneMeasurement(td, sc, pair, port, duration, reversed, cap, bidir);
        }
        td.retryAttempt[pair].store(0);
        return res;
    }

    // Counts finished vs failed pairs in the current cycle and pushes them to the
    // running-totals line. DONE = completed this cycle so far; FAILED = dropped/errored.
    static void pushTotals(PairCycle *cyc, int n) {
        int done = 0, failed = 0;
        for(int i = 0; i < n; i++) {
            if(cyc[i].status == DONE)        done++;
            else if(cyc[i].status == FAILED) failed++;
        }
        InterfaceUtils::setTotals(done, failed);
    }

    // Pumps the UI (countdown bar, elapsed clock, live rates) and polls for 'q' until
    // every future is ready. On stop, force-kills all iperf3/ssh processes so the current
    // measurements return promptly; the interrupted cycle is discarded by the caller.
    static void pumpFutures(std::vector<std::future<IperfExecutor::IperfResult>> &futs,
                            testData &td, int numPairs, std::atomic<bool> &stop,
                            const std::string &barLabel, int totalSeconds, bool showRates,
                            int maxRetries) {
        auto start = std::chrono::steady_clock::now();
        bool killed = false;
        std::vector<bool> wasRetrying(numPairs, false);
        while(true) {
            bool allDone = true;
            for(auto &f : futs) {
                if(f.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
                    allDone = false;
                    break;
                }
            }
            if(allDone) break;

            if(InterfaceUtils::pollStopKey()) stop = true;
            if(stop && !killed) {
                IperfExecutor::stopAllIperf3();
                killed = true;
            }

            if(showRates) {
                for(int i = 0; i < numPairs; i++) {
                    if(td.currentTxRate[i] != -1)
                        InterfaceUtils::updatePairRate(i, false, std::to_string((int)td.currentTxRate[i]), InterfaceUtils::CP_TEAL);
                    if(td.currentRxRate[i] != -1)
                        InterfaceUtils::updatePairRate(i, true, std::to_string((int)td.currentRxRate[i]), InterfaceUtils::CP_TEAL);
                }
            }

            // Surface setup-error retries published by the worker threads as an amber
            // "retry N/M" status cell; restore RUNNING once the retry resolves.
            for(int i = 0; i < numPairs; i++) {
                int ra = td.retryAttempt[i].load();
                if(ra > 0) {
                    InterfaceUtils::updatePairRetry(i, ra, maxRetries);
                    wasRetrying[i] = true;
                } else if(wasRetrying[i]) {
                    InterfaceUtils::updatePairStatus(i, RUNNING);
                    wasRetrying[i] = false;
                }
            }

            int elapsed = (int)std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            if(elapsed > totalSeconds) elapsed = totalSeconds;
            int pct = totalSeconds > 0 ? (elapsed * 100 / totalSeconds) : 100;
            std::string sub = std::to_string(elapsed) + "s / " + std::to_string(totalSeconds) + "s";
            InterfaceUtils::drawProgressBar(stop ? (barLabel + " (stopping)") : barLabel, pct, sub);
            InterfaceUtils::refreshElapsed();

            usleep(150 * 1000);
        }
    }

    // Flags this cycle as one where the pair had no working link — whether it dropped
    // mid-stream or couldn't connect at all. Counted once per cycle even if it happens in
    // both Phase A and Phase B. The running count is committed in evaluateAndTally only when
    // the cycle completes (so a cycle discarded by 'q' can't run the count ahead of
    // cyclesCompleted); the provisional count is shown on screen immediately for feedback.
    static void markDropCycle(RunSummary &sum, PairCycle *cyc, int pair) {
        if(!cyc[pair].dropped) {
            cyc[pair].dropped = true;
            InterfaceUtils::updatePairDrops(pair, sum.dropCycles[pair] + 1);
        }
        cyc[pair].status = FAILED;
    }

    static void recordDrop(RunSummary &sum, PairCycle *cyc, int pair, const std::string &where) {
        markDropCycle(sum, cyc, pair);
        sum.dropLog.push_back("cycle " + std::to_string(sum.cyclesCompleted + 1) +
            " · pair " + std::to_string(pair + 1) +
            " · connection lost (" + where + ") @" + timestampNow());
    }
    // A measurement that couldn't start (after retries) means the pair had no working link
    // this cycle, so it counts as a drop-cycle too — recorded here and in the Errors log.
    static void recordError(RunSummary &sum, PairCycle *cyc, int pair, const std::string &where) {
        markDropCycle(sum, cyc, pair);
        sum.errorLog.push_back("cycle " + std::to_string(sum.cyclesCompleted + 1) +
            " · pair " + std::to_string(pair + 1) +
            " · could not start (" + where + ") @" + timestampNow());
    }

    // Phase A: max throughput, one pair at a time, TX then RX (both judged vs targets).
    static void phaseA(testData &td, ServerConfigurationLoader::ServerConfiguration *sc,
                       ConfigureTest::testConfiguration *tc, PairCycle *cyc,
                       std::atomic<bool> &stop, RunSummary &sum) {
        int n = tc->numOfPairs;
        int dur = sc->phaseDuration;
        InterfaceUtils::setPhaseLine("Phase A  max throughput - one pair at a time, " +
            std::to_string(dur) + "s each direction");
        for(int pair = 0; pair < n && !stop; pair++) {
            cyc[pair].status = RUNNING;
            InterfaceUtils::updatePairStatus(pair, RUNNING);
            for(int dir = 0; dir < 2 && !stop; dir++) {
                bool reversed = (dir == 1);
                int port = PORT_START + pair;

                std::vector<std::future<IperfExecutor::IperfResult>> futs;
                futs.push_back(std::async(std::launch::async, [&td, sc, pair, port, dur, reversed, &stop]() {
                    return measureWithRetry(td, sc, pair, port, dur, reversed, 0, false, sc->retries, stop);
                }));

                std::string label = reversed
                    ? ("Pair " + std::to_string(pair + 1) + " H->F")
                    : ("Pair " + std::to_string(pair + 1) + " F->H");
                pumpFutures(futs, td, n, stop, label, dur, true, sc->retries);

                IperfExecutor::IperfResult res = futs[0].get();
                if(stop) { return; } // discard interrupted cycle

                if(res == IperfExecutor::IPERF_RESULT_COMPLETED) {
                    // Paint the final value colored against its target (green pass / red miss).
                    if(reversed) {
                        cyc[pair].rxRate = td.averageRxRate[pair];
                        int color = cyc[pair].rxRate >= sc->rxTargetSpeed ? InterfaceUtils::CP_GREEN : InterfaceUtils::CP_RED;
                        InterfaceUtils::updatePairRate(pair, true, std::to_string((int)cyc[pair].rxRate), color);
                    } else {
                        cyc[pair].txRate = td.averageTxRate[pair];
                        int color = cyc[pair].txRate >= sc->txTargetSpeed ? InterfaceUtils::CP_GREEN : InterfaceUtils::CP_RED;
                        InterfaceUtils::updatePairRate(pair, false, std::to_string((int)cyc[pair].txRate), color);
                    }
                } else if(res == IperfExecutor::IPERF_RESULT_CONNECTION_DROP) {
                    recordDrop(sum, cyc, pair, reversed ? "Phase A H->F" : "Phase A F->H");
                    InterfaceUtils::updatePairRate(pair, reversed, "drop", InterfaceUtils::CP_RED);
                } else {
                    recordError(sum, cyc, pair, reversed ? "Phase A H->F" : "Phase A F->H");
                    InterfaceUtils::updatePairRate(pair, reversed, "err", InterfaceUtils::CP_RED);
                }
            }
            if(cyc[pair].status != FAILED) cyc[pair].status = DONE;
            InterfaceUtils::updatePairStatus(pair, cyc[pair].status);
            pushTotals(cyc, n);
        }
    }

    // Runs every pair in parallel in ONE direction for `dur` seconds, capped, recording
    // any connection drops. Used for each half of Phase B. The live rate lands in the TX
    // column for the forward half and the RX column for the reverse half.
    static void soakHalf(testData &td, ServerConfigurationLoader::ServerConfiguration *sc,
                         ConfigureTest::testConfiguration *tc, PairCycle *cyc,
                         std::atomic<bool> &stop, RunSummary &sum,
                         bool reversed, int dur, int cap, const std::string &dirLabel) {
        int n = tc->numOfPairs;
        for(int pair = 0; pair < n; pair++) {
            // Clear the live-rate variables synchronously (before launching), so the pump
            // can't paint a stale value (e.g. Phase A's RX) into the column this half
            // doesn't measure.
            td.currentTxRate[pair] = -1;
            td.currentRxRate[pair] = -1;
            InterfaceUtils::updatePairRate(pair, reversed, "...", InterfaceUtils::CP_DIM);
            // A pair that already failed in Phase A stays FAILED; others soak as RUNNING.
            if(cyc[pair].status != FAILED) InterfaceUtils::updatePairStatus(pair, RUNNING);
        }

        std::vector<std::future<IperfExecutor::IperfResult>> futs;
        for(int pair = 0; pair < n; pair++) {
            int port = PORT_START + pair;
            futs.push_back(std::async(std::launch::async, [&td, sc, pair, port, dur, cap, reversed, &stop]() {
                return measureWithRetry(td, sc, pair, port, dur, reversed, cap, false, sc->retries, stop);
            }));
        }

        std::string label = "Soak " + dirLabel + " @" + std::to_string(cap) + "m";
        pumpFutures(futs, td, n, stop, label, dur, true, sc->retries);

        for(int pair = 0; pair < n; pair++) {
            IperfExecutor::IperfResult res = futs[pair].get();
            if(stop) continue; // discard interrupted cycle
            if(res == IperfExecutor::IPERF_RESULT_CONNECTION_DROP) {
                recordDrop(sum, cyc, pair, "Phase B " + dirLabel);
                InterfaceUtils::updatePairRate(pair, reversed, "drop", InterfaceUtils::CP_RED);
            } else if(res == IperfExecutor::IPERF_RESULT_SETUP_ERROR) {
                recordError(sum, cyc, pair, "Phase B " + dirLabel);
                InterfaceUtils::updatePairRate(pair, reversed, "err", InterfaceUtils::CP_RED);
            } else {
                // Completed with no drop: keep the last live rate, mark green.
                if(td.currentTxRate[pair] >= 0 || td.currentRxRate[pair] >= 0) {
                    float v = reversed ? td.currentRxRate[pair] : td.currentTxRate[pair];
                    if(v >= 0) InterfaceUtils::updatePairRate(pair, reversed, std::to_string((int)v), InterfaceUtils::CP_GREEN);
                }
            }
            InterfaceUtils::updatePairStatus(pair, cyc[pair].status);
            pushTotals(cyc, n);
        }
    }

    // Phase B: capped soak of all pairs in parallel — forward (TX) half then reverse (RX)
    // half, each soakDuration/2. Judged only on connection drops. The EdgeRouter's iperf3
    // has no --bidir, so the two directions run one after another rather than at once.
    static void phaseB(testData &td, ServerConfigurationLoader::ServerConfiguration *sc,
                       ConfigureTest::testConfiguration *tc, PairCycle *cyc,
                       std::atomic<bool> &stop, RunSummary &sum) {
        int half = sc->soakDuration / 2;
        if(half < 1) half = 1;
        int cap = sc->soakCap;

        InterfaceUtils::setPhaseLine("Phase B  parallel soak - all pairs at once, capped " +
            std::to_string(cap) + " Mbps, F->H then H->F");
        for(int pair = 0; pair < tc->numOfPairs; pair++) {
            InterfaceUtils::updatePairRate(pair, false, "-", InterfaceUtils::CP_DIM);
            InterfaceUtils::updatePairRate(pair, true, "-", InterfaceUtils::CP_DIM);
        }

        soakHalf(td, sc, tc, cyc, stop, sum, false, half, cap, "F->H"); // client -> server
        if(stop) return;
        soakHalf(td, sc, tc, cyc, stop, sum, true, half, cap, "H->F");  // server -> client (-R)
    }

    // Scores each pair independently for this cycle: throughput (Phase A) vs targets,
    // plus whether it dropped/errored. Updates cumulative counters and peaks.
    static void evaluateAndTally(ServerConfigurationLoader::ServerConfiguration *sc,
                                 int n, PairCycle *cyc, RunSummary &sum) {
        for(int pair = 0; pair < n; pair++) {
            // Commit this completed cycle's drop into the running count.
            if(cyc[pair].dropped) sum.dropCycles[pair]++;

            if(cyc[pair].txRate > sum.peakTx[pair]) sum.peakTx[pair] = cyc[pair].txRate;
            if(cyc[pair].rxRate > sum.peakRx[pair]) sum.peakRx[pair] = cyc[pair].rxRate;

            bool throughputOk = (cyc[pair].txRate >= sc->txTargetSpeed) &&
                                (cyc[pair].rxRate >= sc->rxTargetSpeed);
            bool cyclePass = throughputOk && (cyc[pair].status != FAILED);
            if(cyclePass) sum.passCount[pair]++;
            else          sum.failCount[pair]++;
        }
    }

    bool pairPassed(const RunSummary &sum, int pair) {
        // A pair fails a cycle if it misses throughput OR drops (a drop marks the cycle
        // FAILED), so failCount already captures both — any single drop fails the pair.
        return sum.failCount[pair] == 0;
    }

    void runContinuous(ConfigureTest::testConfiguration *tc,
                       ServerConfigurationLoader::ServerConfiguration *sc,
                       testData &td, RunSummary &sum) {
        int n = tc->numOfPairs;
        sum.startTime = timestampNow();

        if((int)sc->clientIps.size() < n || (int)sc->serverIps.size() < n) {
            InterfaceUtils::createNewPage("Configuration error",
                "Not enough IPs configured for the requested number of pairs.\n"
                "Check config/clientIps.conf and config/serverIps.conf.",
                "Press any key to exit");
            getch();
            sum.endTime = timestampNow();
            return;
        }

        InterfaceUtils::setNonBlockingInput(true);
        std::atomic<bool> stop(false);

        // Static context for the live monitor (drawn fresh each cycle).
        InterfaceUtils::MonitorInfo info;
        info.operatorInitials = tc->operatorInitials;
        info.configName       = tc->configurationName;
        info.numPairs         = n;
        info.txTarget         = sc->txTargetSpeed;
        info.rxTarget         = sc->rxTargetSpeed;
        info.serials          = tc->serialNumberPairs;

        while(!stop) {
            // Fresh live data + per-cycle scratch. Clearing logs each cycle keeps
            // memory bounded over a long run (the report keeps the last cycle's logs).
            for(int i = 0; i < MAX_PAIRS; i++) {
                td.currentRxRate[i] = td.currentTxRate[i] = -1;
                td.averageRxRate[i] = td.averageTxRate[i] = -1;
                td.testLogs[i] = "";
                td.retryAttempt[i].store(0);
            }
            PairCycle cyc[MAX_PAIRS];

            InterfaceUtils::beginTestMonitor(info);
            InterfaceUtils::setCycle(sum.cyclesCompleted + 1);
            // Carry the running drop-cycle counts across the fresh frame.
            for(int i = 0; i < n; i++) InterfaceUtils::updatePairDrops(i, sum.dropCycles[i]);

            phaseA(td, sc, tc, cyc, stop, sum);
            if(stop) break;

            phaseB(td, sc, tc, cyc, stop, sum);
            if(stop) break;

            evaluateAndTally(sc, n, cyc, sum);
            sum.cyclesCompleted++;
        }

        IperfExecutor::stopAllIperf3(); // ensure nothing is left running
        InterfaceUtils::setNonBlockingInput(false);
        sum.endTime = timestampNow();
    }
}
