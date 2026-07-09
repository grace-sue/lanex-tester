#ifndef REPORT_GENERATOR
#define REPORT_GENERATOR
#include "configureTest.h"
#include "testData.h"
#include <fstream>
#include <string>
namespace ReportGenerator {
    bool saveReport(ConfigureTest::testConfiguration *tc);
    bool appendToAllReports(std::string fileContent);
    bool saveEngReport(ConfigureTest::testConfiguration *tc, LANEXTest::testData *td, bool passed);
}
#endif
