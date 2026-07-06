#ifndef TEST_DATA_H
#define TEST_DATA_H
#include <string>
namespace LANEXTest {
    struct testData {
        float currentRxRate[8];
        float currentTxRate[8];
        float averageRxRate[8];
        float averageTxRate[8];
        std::string testLogs[8];
    };
    
}
#endif