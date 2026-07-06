#ifndef CONFIGURE_TEST
#define CONFIGURE_TEST
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>

namespace ConfigureTest {
    struct testConfiguration {
        int numOfPairs;
        int numOfSwitches;
        std::string testId;
        std::string operatorInitials;
        std::string timestamp;
        std::string configurationName;
        std::vector<std::string> serialNumberPairs;
        std::vector<std::string> serialNumberSwitches;
    };

    testConfiguration startTestConfiguration();
}
#endif