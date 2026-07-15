#ifndef IPERF_EXECUTOR
#define IPERF_EXECUTOR
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <cstdlib>
#include <string.h>
#include "testData.h"
#include "test.h"

namespace IperfExecutor {
    std::string ltrim(const std::string &s);
    
    std::string rtrim(const std::string &s);

    enum IperfMessageType {
        IPERF_ERROR,
        IPERF_PROGRESS_STATS,
        IPERF_COMPLETED_SENDER_STATS,
        IPERF_COMPLETED_RECEIVER_STATS,
        IPERF_IGNORE
    };

    // Outcome of a single remote-client run, so the test engine can tell a genuine
    // connection drop apart from a measurement that never got started.
    enum IperfResult {
        IPERF_RESULT_COMPLETED,        // ran to a clean finish ("iperf Done.")
        IPERF_RESULT_CONNECTION_DROP,  // connected and data flowed, then died mid-test
        IPERF_RESULT_SETUP_ERROR       // never connected (ssh / setup failure)
    };

    struct IperfMessage;

    bool checkFirstCharacters(std::array<char, 128> *buffer, const char *key);

    IperfMessage parseIperfBuffer(std::array<char, 128> *buffer);

    // Probes the remote iperf3 version once (over ssh) and remembers whether it supports
    // --forceflush (iperf3 3.1+). Call once before the test loop; the result is cached and
    // then read by startRemoteIperf3Client so old and new iperf3 both work. Safe default
    // (off) if the version can't be determined, since an old iperf3 rejects the flag.
    void detectRemoteCapabilities(std::string remoteAddress);
    bool remoteSupportsForceflush();   // the cached result (exposed for tests)

    // bandwidthCap: Mbps per pair (0 = uncapped). bidir: run both directions at once (--bidir).
    IperfResult startRemoteIperf3Client(int duration, LANEXTest::testData *td, int clientId, std::string remoteAddress,
    std::string serverAddress, int serverPort, bool isReversed, int bandwidthCap = 0, bool bidir = false);

    void startLocalIperf3Server(int serverPort, int timeoutSeconds);

    void stopLocalIperf3Server(int serverPort);

    // Force-stops every local iperf3 server AND every local ssh client process, so a
    // measurement's pipe closes promptly even if the remote client won't exit on its own.
    void stopAllIperf3();
}

#endif
