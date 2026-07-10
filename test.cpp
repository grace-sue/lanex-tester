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
            res = runOneMeasurement(td, sc, pair, port, duration, reversed, cap, bidir);
        }
        return res;
    }

    // Pumps the UI (countdown, and live rates in Phase A) and polls for 'q' until every
    // future is ready. On stop, force-kills all iperf3/ssh processes so the current
    // measurements return promptly; the interrupted cycle is discarded by the caller.
    static void pumpFutures(std::vector<std::future<IperfExecutor::IperfResult>> &futs,
                            testData &td, int numPairs, std::atomic<bool> &stop,
                            const std::string &phaseLabel, int totalSeconds, bool showRates) {
        auto start = std::chrono::steady_clock::now();
        bool killed = false;
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
                    if(td.currentTxRate[i] != -1) InterfaceUtils::updateTxOfPair(i, std::to_string((int)td.currentTxRate[i]));
                    if(td.currentRxRate[i] != -1) InterfaceUtils::updateRxOfPair(i, std::to_string((int)td.currentRxRate[i]));
                }
            }

            int elapsed = (int)std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            int left = totalSeconds - elapsed;
            if(left < 0) left = 0;
            std::string status = stop
                ? (phaseLabel + " | stopping...")
                : (phaseLabel + " | ~" + std::to_string(left) + "s | press q to stop");
            InterfaceUtils::updateProgress(status);

            usleep(150 * 1000);
        }
    }

    static void recordDrop(RunSummary &sum, PairCycle *cyc, int pair, const std::string &where) {
        sum.connectionDrops[pair]++;
        sum.dropLog.push_back("cycle " + std::to_string(sum.cyclesCompleted + 1) +
            " · pair " + std::to_string(pair + 1) +
            " · connection lost (" + where + ") @" + timestampNow());
        cyc[pair].status = FAILED;
    }
    static void recordError(RunSummary &sum, PairCycle *cyc, int pair, const std::string &where) {
        sum.errorLog.push_back("cycle " + std::to_string(sum.cyclesCompleted + 1) +
            " · pair " + std::to_string(pair + 1) +
            " · could not start (" + where + ") @" + timestampNow());
        cyc[pair].status = FAILED;
    }

    // Phase A: max throughput, one pair at a time, TX then RX (both judged vs targets).
    static void phaseA(testData &td, ServerConfigurationLoader::ServerConfiguration *sc,
                       ConfigureTest::testConfiguration *tc, PairCycle *cyc,
                       std::atomic<bool> &stop, RunSummary &sum) {
        int n = tc->numOfPairs;
        int dur = sc->phaseDuration;
        for(int pair = 0; pair < n && !stop; pair++) {
            cyc[pair].status = RUNNING;
            for(int dir = 0; dir < 2 && !stop; dir++) {
                bool reversed = (dir == 1);
                int port = PORT_START + pair;

                std::vector<std::future<IperfExecutor::IperfResult>> futs;
                futs.push_back(std::async(std::launch::async, [&td, sc, pair, port, dur, reversed, &stop]() {
                    return measureWithRetry(td, sc, pair, port, dur, reversed, 0, false, sc->retries, stop);
                }));

                std::string label = "Cycle " + std::to_string(sum.cyclesCompleted + 1) +
                    " | Phase A | Pair " + std::to_string(pair + 1) + "/" + std::to_string(n) +
                    (reversed ? " RX" : " TX");
                pumpFutures(futs, td, n, stop, label, dur, true);

                IperfExecutor::IperfResult res = futs[0].get();
                if(stop) { return; } // discard interrupted cycle

                if(res == IperfExecutor::IPERF_RESULT_COMPLETED) {
                    if(reversed) cyc[pair].rxRate = td.averageRxRate[pair];
                    else         cyc[pair].txRate = td.averageTxRate[pair];
                } else if(res == IperfExecutor::IPERF_RESULT_CONNECTION_DROP) {
                    recordDrop(sum, cyc, pair, reversed ? "Phase A RX" : "Phase A TX");
                } else {
                    recordError(sum, cyc, pair, reversed ? "Phase A RX" : "Phase A TX");
                }
            }
            if(cyc[pair].status != FAILED) cyc[pair].status = DONE;
        }
    }

    // Phase B: capped soak, all pairs in parallel. Judged only on connection drops.
    // Uses a plain capped forward stream (the EdgeRouter's iperf3 has no --bidir);
    // bidirectional coverage is provided by Phase A's TX+RX passes. TX shows the live
    // ~cap rate; RX is not exercised in this phase.
    static void phaseB(testData &td, ServerConfigurationLoader::ServerConfiguration *sc,
                       ConfigureTest::testConfiguration *tc, PairCycle *cyc,
                       std::atomic<bool> &stop, RunSummary &sum) {
        int n = tc->numOfPairs;
        int dur = sc->soakDuration;
        int cap = sc->soakCap;

        for(int pair = 0; pair < n; pair++) {
            InterfaceUtils::updateTxOfPair(pair, "soak");
            InterfaceUtils::updateRxOfPair(pair, "-");
        }

        std::vector<std::future<IperfExecutor::IperfResult>> futs;
        for(int pair = 0; pair < n; pair++) {
            int port = PORT_START + pair;
            futs.push_back(std::async(std::launch::async, [&td, sc, pair, port, dur, cap, &stop]() {
                return measureWithRetry(td, sc, pair, port, dur, false, cap, false, sc->retries, stop);
            }));
        }

        std::string label = "Cycle " + std::to_string(sum.cyclesCompleted + 1) +
            " | Phase B | soak all " + std::to_string(n) + " pairs @" + std::to_string(cap) + "m";
        pumpFutures(futs, td, n, stop, label, dur, true); // TX shows the live capped rate

        for(int pair = 0; pair < n; pair++) {
            IperfExecutor::IperfResult res = futs[pair].get();
            if(stop) continue; // discard interrupted cycle
            if(res == IperfExecutor::IPERF_RESULT_CONNECTION_DROP) {
                recordDrop(sum, cyc, pair, "Phase B");
                InterfaceUtils::updateTxOfPair(pair, "DROP"); InterfaceUtils::updateRxOfPair(pair, "DROP");
            } else if(res == IperfExecutor::IPERF_RESULT_SETUP_ERROR) {
                recordError(sum, cyc, pair, "Phase B");
                InterfaceUtils::updateTxOfPair(pair, "ERR"); InterfaceUtils::updateRxOfPair(pair, "ERR");
            } else {
                InterfaceUtils::updateTxOfPair(pair, "OK"); InterfaceUtils::updateRxOfPair(pair, "-");
            }
        }
    }

    // Scores each pair independently for this cycle: throughput (Phase A) vs targets,
    // plus whether it dropped/errored. Updates cumulative counters and peaks.
    static void evaluateAndTally(ServerConfigurationLoader::ServerConfiguration *sc,
                                 int n, PairCycle *cyc, RunSummary &sum) {
        for(int pair = 0; pair < n; pair++) {
            if(cyc[pair].txRate > sum.peakTx[pair]) sum.peakTx[pair] = cyc[pair].txRate;
            if(cyc[pair].rxRate > sum.peakRx[pair]) sum.peakRx[pair] = cyc[pair].rxRate;

            bool throughputOk = (cyc[pair].txRate >= sc->txTargetSpeed) &&
                                (cyc[pair].rxRate >= sc->rxTargetSpeed);
            bool cyclePass = throughputOk && (cyc[pair].status != FAILED);
            if(cyclePass) sum.passCount[pair]++;
            else          sum.failCount[pair]++;
        }
    }

    bool pairPassed(const RunSummary &sum, int pair, int maxConnDrops) {
        return sum.failCount[pair] == 0 && sum.connectionDrops[pair] <= maxConnDrops;
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

        while(!stop) {
            // Fresh live data + per-cycle scratch. Clearing logs each cycle keeps
            // memory bounded over a long run (the report keeps the last cycle's logs).
            for(int i = 0; i < MAX_PAIRS; i++) {
                td.currentRxRate[i] = td.currentTxRate[i] = -1;
                td.averageRxRate[i] = td.averageTxRate[i] = -1;
                td.testLogs[i] = "";
            }
            PairCycle cyc[MAX_PAIRS];

            InterfaceUtils::createNewTestMonitorPage(tc);

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
