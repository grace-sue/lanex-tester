#ifndef IPERF_EXECUTOR
#define IPERF_EXECUTOR
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
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

    struct IperfMessage;

    bool checkFirstCharacters(std::array<char, 128> *buffer, const char *key);

    IperfMessage parseIperfBuffer(std::array<char, 128> *buffer);

    void startRemoteIperf3Client(int duration, LANEXTest::testData *td, int clientId, std::string remoteAddress, 
    std::string serverAddress, int serverPort, bool isReversed);

    void startLocalIperf3Server(int serverPort);
}

#endif