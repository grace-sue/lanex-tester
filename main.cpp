#include "iperfExecutor.h"
#include "pingExecutor.h"
#include "interfaceUtils.h"
#include <ncurses.h>
#include "configureTest.h"
#include <vector>
#include "test.h"
#include "serverConfigurationLoader.h"
#include "reportGenerator.h"
#include "testData.h"

int main() {

    InterfaceUtils::initScreen();

    // Load server configuration
    ServerConfigurationLoader::ServerConfiguration serverConf = ServerConfigurationLoader::loadConfiguration();

    // Get test configuration
    ConfigureTest::testConfiguration tc = ConfigureTest::startTestConfiguration();

    // Ping all devices
    PingExecutor::pingAllTestWindow(tc.numOfPairs, &serverConf.clientIps);
    
    // Start test
    LANEXTest::testData testData;
    bool testPassed = LANEXTest::startTest(&tc, &serverConf, testData);

    // Save eng report
    bool reportSaved = ReportGenerator::saveEngReport(&tc, &testData, testPassed);
    if(!reportSaved) {
        InterfaceUtils::createNewPage("Error!", "ERROR: The report could not be saved", "Press any key to exit");
        getch();
    }

    // Save report if passed
    if(testPassed) {
        bool reportSaved = ReportGenerator::saveReport(&tc);
        if(!reportSaved) {
            InterfaceUtils::createNewPage("Error!", "ERROR: The report could not be saved", "Press any key to exit");
            getch();
        }
    }

    
    InterfaceUtils::endScreen();

    return 0;
}