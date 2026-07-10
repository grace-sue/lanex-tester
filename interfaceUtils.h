#ifndef INTERFACE_UTILS
#define INTERFACE_UTILS
#include <ncurses.h>
#include <string>
#include "configureTest.h"

namespace InterfaceUtils {
    void initScreen();
    void endScreen();
    void createNewPage(std::string pageTitle, std::string pageDescription, std::string pageQuestion);
    void getStringFromCin(std::string &str);
    void createNewTestMonitorPage(ConfigureTest::testConfiguration *tc);
    void updateTxOfPair(int pairN, std::string val);
    void updateRxOfPair(int pairN, std::string val);
    void updateProgress(std::string val);
    void setNonBlockingInput(bool nonBlocking);
    bool pollStopKey();   // true if 'q'/'Q' is waiting in the input buffer (non-blocking)
}
#endif