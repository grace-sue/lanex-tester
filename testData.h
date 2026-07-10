#ifndef TEST_DATA_H
#define TEST_DATA_H
#include <string>
#include <vector>
#include <atomic>
#include "constants.h"
namespace LANEXTest {
    // Live per-pair rates used by the current UI update loop.
    // (Retained as-is; the v2 engine will migrate onto PairCycle/RunSummary below.)
    struct testData {
        float currentRxRate[MAX_PAIRS];
        float currentTxRate[MAX_PAIRS];
        float averageRxRate[MAX_PAIRS];
        float averageTxRate[MAX_PAIRS];
        std::string testLogs[MAX_PAIRS];
        // Set by the measurement worker thread, read by the UI pump thread: 0 = not
        // retrying, N = setup-error retry attempt N in progress (drives the amber
        // "retry N/M" status cell). Atomic so the cross-thread read is safe.
        std::atomic<int> retryAttempt[MAX_PAIRS];
    };

    // Per-pair status shown on the live monitor.
    enum PairStatus { WAITING, RUNNING, DONE, RETRYING, FAILED };

    // Results for one pair within a single test cycle (reset each cycle).
    struct PairCycle {
        float txRate = -1, rxRate = -1;   // Mbps, Phase A peaks
        float soakTx = -1, soakRx = -1;   // Mbps, Phase B (live only, not scored)
        PairStatus status = WAITING;
    };

    // Accumulated results for the whole run, written to the report on stop.
    struct RunSummary {
        int   cyclesCompleted = 0;
        int   passCount[MAX_PAIRS] = {0};
        int   failCount[MAX_PAIRS] = {0};
        bool  dropped[MAX_PAIRS] = {false};       // pair lost its link at least once (any drop = fail)
        float peakTx[MAX_PAIRS] = {0}, peakRx[MAX_PAIRS] = {0};
        std::vector<std::string> dropLog;    // timestamped connection-drop events
        std::vector<std::string> errorLog;   // transient / setup errors
        std::string startTime, endTime;
        std::string engLog;                  // capped raw iperf logs
    };

}
#endif
