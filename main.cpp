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
    InterfaceUtils::createNewPage("Run Summary", summary, "Press any key to save the reports");
    getch();

    bool overallPass = (sum.cyclesCompleted > 0 && passedPairs == tc.numOfPairs);

    // Save the per-pair summary report and the engineering report (raw logs, last cycle).
    std::string summaryPath, engPath;
    bool summarySaved = ReportGenerator::saveSummaryReport(&tc, sum, summaryPath);
    bool engSaved = ReportGenerator::saveEngReport(&tc, &testData, overallPass, engPath);

    // Final screen: tell the operator exactly where the reports were saved.
    if(summarySaved && engSaved) {
        std::string savedMsg = "The reports have been saved.\n\n";
        savedMsg += "Summary report:\n    " + summaryPath + "\n\n";
        savedMsg += "Engineering report:\n    " + engPath + "\n\n";
        savedMsg += "(paths are relative to the tester folder)";
        InterfaceUtils::createNewPage("Reports Saved", savedMsg, "Press any key to exit");
    } else {
        std::string errMsg = "ERROR: The report could not be fully saved.\n\n";
        errMsg += std::string("Summary report:     ") + (summarySaved ? summaryPath : "NOT SAVED") + "\n";
        errMsg += std::string("Engineering report: ") + (engSaved ? engPath : "NOT SAVED") + "\n";
        InterfaceUtils::createNewPage("Error!", errMsg, "Press any key to exit");
    }
    getch();

    InterfaceUtils::endScreen();

    return 0;
}