#include "reportGenerator.h"
#include "test.h"          // LANEXTest::pairPassed — keep the verdict rule in one place
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

namespace ReportGenerator {

    bool ensureDirectory(std::string path) {
        if(path.empty()) {
            return true;
        }

        if(mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) {
            return true;
        }

        return false;
    }

    bool ensureReportDirectories() {
        return ensureDirectory("reports") && ensureDirectory("reports/eng");
    }

    std::string sanitizeFileName(std::string fileName) {
        for(int i = 0; i < fileName.size(); i++) {
            char currChar = fileName[i];
            if(currChar == '/' || currChar == '\\' || currChar == ':' ||
                currChar == '*' || currChar == '?' || currChar == '"' ||
                currChar == '<' || currChar == '>' || currChar == '|' ||
                currChar == '\r' || currChar == '\n') {
                fileName[i] = '_';
            }
        }

        return fileName;
    }
    

    bool saveFile(std::string fileContent, std::string fileName) {
        if(!ensureReportDirectories()) {
            return false;
        }

        std::ofstream file;
        std::string outLocation = "reports/";
        std::string directoryPrefix;
        size_t lastSlash = fileName.find_last_of("/");
        if(lastSlash != std::string::npos) {
            directoryPrefix = fileName.substr(0, lastSlash + 1);
            fileName = fileName.substr(lastSlash + 1);
        }

        fileName = directoryPrefix + sanitizeFileName(fileName);
        outLocation += fileName;
        outLocation += ".txt";
        file.open(outLocation);
        if(file.is_open()) {
            file << fileContent;
            file.close();
            return true;
        }
        return false;
    }

    bool appendToAllReports(std::string fileContent) {
        if(!ensureReportDirectories()) {
            return false;
        }

        std::string outCnt = "\n---------------------------------------\n";
        outCnt += fileContent;
        std::ofstream file("reports/allReports.txt", std::ios_base::app);
        if(file.is_open()) {
            file << outCnt;
            file.close();
            return true;
        } 
        return false;
    }

    // Compacts a "YYYY-MM-DD HH:MM:SS" timestamp into "YYYYMMDD_HHMMSS" for use in a
    // file name (no spaces or colons). Falls back gracefully on an unexpected format.
    static std::string compactTimestamp(const std::string &ts) {
        std::string digits;
        for(char c : ts) {
            if(c >= '0' && c <= '9') digits += c;
        }
        if(digits.size() >= 14) {
            return digits.substr(0, 8) + "_" + digits.substr(8, 6);
        }
        return digits.empty() ? "nodate" : digits;
    }

    // Builds an operator-meaningful report file name: how many pairs, which unit was tested
    // (the first pair's serial), who ran it (operator initials), and when (run timestamp).
    // The timestamp keeps each run's file unique instead of overwriting a previous one.
    static std::string reportBaseName(ConfigureTest::testConfiguration *tc) {
        std::string count = std::to_string(tc->numOfPairs) + "pairs";
        std::string unit = tc->serialNumberPairs.empty() ? "run" : tc->serialNumberPairs[0];
        std::string op = tc->operatorInitials.empty() ? "NA" : tc->operatorInitials;
        return count + "_" + unit + "_" + op + "_" + compactTimestamp(tc->timestamp);
    }

    std::string buildRunSummaryText(ConfigureTest::testConfiguration *tc,
                                    const LANEXTest::RunSummary &sum, int &passedPairsOut) {
        passedPairsOut = 0;
        if(sum.cyclesCompleted == 0) {
            return "No complete test cycles were run.";
        }

        std::string out;
        out += "Started: " + sum.startTime + "\n";
        out += "Ended:   " + sum.endTime + "\n";
        out += "Cycles completed: " + std::to_string(sum.cyclesCompleted) + "\n\n";
        for(int i = 0; i < tc->numOfPairs; i++) {
            bool p = LANEXTest::pairPassed(sum, i);
            if(p) passedPairsOut++;
            out += "Pair " + std::to_string(i + 1) + " (" + tc->serialNumberPairs[i] + "): " +
                   (p ? "PASS" : "FAIL") +
                   "   peak TX " + std::to_string((int)sum.peakTx[i]) +
                   " / RX " + std::to_string((int)sum.peakRx[i]) + " Mbps" +
                   "   drops: " + std::to_string(sum.dropCycles[i]) + "\n";
        }
        out += "\n" + std::to_string(passedPairsOut) + " / " +
               std::to_string(tc->numOfPairs) + " pairs passed";
        return out;
    }

    bool saveSummaryReport(ConfigureTest::testConfiguration *tc, const LANEXTest::RunSummary &sum) {
        int passedPairs = 0;
        std::string body = buildRunSummaryText(tc, sum, passedPairs);

        std::string fileContent = "LAN-EX H/F Tester - Summary Report\n\n";
        fileContent += "Operator:      " + tc->operatorInitials + "\n";
        fileContent += "Configuration: " + tc->configurationName + "\n\n";
        fileContent += body + "\n\n";

        // Documented failures: connection drops and setup errors captured during the run.
        fileContent += "Connection drops\n--------------------------------------\n";
        if(sum.dropLog.empty()) {
            fileContent += "(none)\n";
        } else {
            for(const std::string &line : sum.dropLog) fileContent += line + "\n";
        }
        fileContent += "\nErrors\n--------------------------------------\n";
        if(sum.errorLog.empty()) {
            fileContent += "(none)\n";
        } else {
            for(const std::string &line : sum.errorLog) fileContent += line + "\n";
        }

        if(!saveFile(fileContent, reportBaseName(tc))) {
            return false;
        }
        return appendToAllReports(fileContent);
    }

    bool saveEngReport(ConfigureTest::testConfiguration *tc, LANEXTest::testData *td, bool passed) {
        std::string fileContent = "";

        fileContent += "LAN-EX F/H Tester Report\n\n";
        fileContent += "Date: ";
        fileContent += tc->timestamp;
        fileContent += "\nOperator Initials: ";
        fileContent += tc->operatorInitials;
        fileContent += "\nConfiguration name: ";
        fileContent += tc->configurationName;
        

        if(passed) {
            fileContent += "\nResult: PASSED\n";
        } else {
            fileContent += "\nResult: FAILED\n";
        }

        fileContent += "\n\n";

        for(int i = 0; i < tc->numOfPairs; i++) {
            fileContent += "--------------------------------------\nLogs for pair ";
            fileContent += std::to_string(i + 1);
            fileContent += " (";
            fileContent += tc->serialNumberPairs[i];
            fileContent += ")\n";
            fileContent += td->testLogs[i];
            fileContent += "\n\n";

        }

        return saveFile(fileContent, "eng/eng_" + reportBaseName(tc));
    }


}
