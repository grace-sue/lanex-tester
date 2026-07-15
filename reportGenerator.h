#ifndef REPORT_GENERATOR
#define REPORT_GENERATOR
#include "configureTest.h"
#include "testData.h"
#include <fstream>
#include <string>
namespace ReportGenerator {
    // Compact per-pair verdict text, shared by the on-screen result and the summary
    // file so they can never disagree. Sets passedPairsOut to the pass count.
    std::string buildRunSummaryText(ConfigureTest::testConfiguration *tc,
                                    const LANEXTest::RunSummary &sum, int &passedPairsOut);
    // Full summary report file (accountable header + per-pair verdicts + drop/error log),
    // also appended to reports/allReports.txt. savedPath receives the file path written.
    bool saveSummaryReport(ConfigureTest::testConfiguration *tc, const LANEXTest::RunSummary &sum,
                           std::string &savedPath);
    bool appendToAllReports(std::string fileContent);
    bool saveEngReport(ConfigureTest::testConfiguration *tc, LANEXTest::testData *td, bool passed,
                       std::string &savedPath);
}
#endif
