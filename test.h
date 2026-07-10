#ifndef TEST_H
#define TEST_H
#include "configureTest.h"
#include "interfaceUtils.h"
#include "testData.h"
#include "iperfExecutor.h"
#include "serverConfigurationLoader.h"
#include "pingExecutor.h"
namespace LANEXTest {

    // Runs the continuous two-phase test loop until the operator presses 'q'.
    // Fills `sum` with the accumulated per-pair results for the report.
    void runContinuous(ConfigureTest::testConfiguration *tc,
                       ServerConfigurationLoader::ServerConfiguration *serverConf,
                       testData &td, RunSummary &sum);

    // Final per-pair verdict: passed throughput every cycle AND stayed within the drop limit.
    bool pairPassed(const RunSummary &sum, int pair, int maxConnDrops);
};
#endif
