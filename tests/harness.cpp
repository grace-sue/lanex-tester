// Test harness for offline verification of Milestones 1 & 2.
// Exposes the config loader and the remote-client runner as a tiny CLI so a
// shell script can assert their behaviour without any real hardware.
//
//   harness config                      -> prints the parsed ServerConfiguration
//   harness iperf <cap> <bidir> <rev>   -> runs startRemoteIperf3Client, prints result
//
// For the `iperf` mode, put a fake `ssh` earlier on PATH to feed canned output.
#include "serverConfigurationLoader.h"
#include "iperfExecutor.h"
#include "testData.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace LANEXTest;

static const char* resultName(IperfExecutor::IperfResult r) {
    switch (r) {
        case IperfExecutor::IPERF_RESULT_COMPLETED:       return "COMPLETED";
        case IperfExecutor::IPERF_RESULT_CONNECTION_DROP: return "CONNECTION_DROP";
        default:                                          return "SETUP_ERROR";
    }
}

int main(int argc, char** argv) {
    if (argc >= 2 && strcmp(argv[1], "config") == 0) {
        auto c = ServerConfigurationLoader::loadConfiguration();
        printf("tx=%.0f rx=%.0f phaseDuration=%d soakDuration=%d soakCap=%d maxConnDrops=%d retries=%d\n",
               c.txTargetSpeed, c.rxTargetSpeed, c.phaseDuration, c.soakDuration,
               c.soakCap, c.maxConnDrops, c.retries);
        return 0;
    }
    if (argc >= 5 && strcmp(argv[1], "iperf") == 0) {
        int cap = atoi(argv[2]);
        bool bidir = atoi(argv[3]) != 0;
        bool rev = atoi(argv[4]) != 0;
        testData td;
        for (int i = 0; i < MAX_PAIRS; i++) {
            td.currentRxRate[i] = td.currentTxRate[i] = td.averageRxRate[i] = td.averageTxRate[i] = -1;
        }
        IperfExecutor::IperfResult r =
            IperfExecutor::startRemoteIperf3Client(5, &td, 0, "10.0.0.1", "10.0.0.2", 5201, rev, cap, bidir);
        printf("%s\n", resultName(r));
        return 0;
    }
    fprintf(stderr, "usage: harness config | harness iperf <cap> <bidir> <rev>\n");
    return 2;
}
