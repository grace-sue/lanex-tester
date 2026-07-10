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

    // Run the continuous two-phase test loop until the operator presses 'q'
    LANEXTest::testData testData;
    LANEXTest::RunSummary sum;
    LANEXTest::runContinuous(&tc, &serverConf, testData, sum);

    // Build the per-pair summary (shared with the report file so they can't disagree).
    int passedPairs = 0;
    std::string summary = ReportGenerator::buildRunSummaryText(&tc, sum, passedPairs);
    InterfaceUtils::createNewPage("Run Summary", summary, "Press any key to save log & exit");
    getch();

    bool overallPass = (sum.cyclesCompleted > 0 && passedPairs == tc.numOfPairs);

    // Save the per-pair summary report and the engineering report (raw logs, last cycle).
    bool summarySaved = ReportGenerator::saveSummaryReport(&tc, sum);
    bool engSaved = ReportGenerator::saveEngReport(&tc, &testData, overallPass);
    if(!summarySaved || !engSaved) {
        InterfaceUtils::createNewPage("Error!", "ERROR: The report could not be saved", "Press any key to exit");
        getch();
    }

    InterfaceUtils::endScreen();

    return 0;
}