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

    // Build the per-pair summary
    std::string summary;
    int passedPairs = 0;
    if(sum.cyclesCompleted == 0) {
        summary = "No complete test cycles were run.";
    } else {
        summary += "Started: " + sum.startTime + "\n";
        summary += "Ended:   " + sum.endTime + "\n";
        summary += "Cycles completed: " + std::to_string(sum.cyclesCompleted) + "\n\n";
        for(int i = 0; i < tc.numOfPairs; i++) {
            bool p = LANEXTest::pairPassed(sum, i, serverConf.maxConnDrops);
            if(p) passedPairs++;
            summary += "Pair " + std::to_string(i + 1) + " (" + tc.serialNumberPairs[i] + "): " +
                       (p ? "PASS" : "FAIL") +
                       "   peakTX=" + std::to_string((int)sum.peakTx[i]) +
                       " peakRX=" + std::to_string((int)sum.peakRx[i]) +
                       " drops=" + std::to_string(sum.connectionDrops[i]) + "\n";
        }
        summary += "\n" + std::to_string(passedPairs) + " / " +
                   std::to_string(tc.numOfPairs) + " pairs passed";
    }
    InterfaceUtils::createNewPage("Run Summary", summary, "Press any key to save log & exit");
    getch();

    bool overallPass = (sum.cyclesCompleted > 0 && passedPairs == tc.numOfPairs);

    // Save engineering report (raw logs from the last cycle).
    // NOTE: the per-pair summary report file is produced in Milestone 6.
    bool reportSaved = ReportGenerator::saveEngReport(&tc, &testData, overallPass);
    if(!reportSaved) {
        InterfaceUtils::createNewPage("Error!", "ERROR: The report could not be saved", "Press any key to exit");
        getch();
    }

    InterfaceUtils::endScreen();

    return 0;
}