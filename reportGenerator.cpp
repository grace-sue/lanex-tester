#include "reportGenerator.h"
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

    bool saveReport(ConfigureTest::testConfiguration *tc) {
        std::string fileContent = "";

        fileContent += "LAN-EX F/H Tester Report\n\n";
        fileContent += "Date: ";
        fileContent += tc->timestamp;
        fileContent += "\nOperator Initials: ";
        fileContent += tc->operatorInitials;
        fileContent += "\nConfiguration name: ";
        fileContent += tc->configurationName;
        fileContent += "\n\n";

        // Pairs
        for(int i = 0; i < tc->numOfPairs; i++) {
            fileContent += "Pair ";
            fileContent += std::to_string(i + 1);
            fileContent += " (";
            fileContent += tc->serialNumberPairs[i];
            fileContent += "): PASSED\n";
        }

        // Switches
        for(int i = 0; i < tc->numOfSwitches; i++) {
            fileContent += "Switch ";
            fileContent += std::to_string(i + 1);
            fileContent += " (";
            fileContent += tc->serialNumberSwitches[i];
            fileContent += "): PASSED\n";
        }

        std::string fileName = tc->configurationName;

        if(tc->numOfPairs == 1) {
            // <configurationName_headSN-fieldSN>
            fileName += "_";
            fileName += tc->serialNumberPairs[0];
        } else {
            // <configurationName_switchSN>
            fileName += "_";
            fileName += tc->serialNumberSwitches[0];
        }

        if(!saveFile(fileContent, fileName)) {
            return false;
        }

        if(!appendToAllReports(fileContent)) {
            return false;
        }

        return true;
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

        // File name
        std::string fileName = "eng/eng_";
        fileName += tc->configurationName;

        if(tc->numOfPairs == 1) {
            // <configurationName_headSN-fieldSN>
            fileName += "_";
            fileName += tc->serialNumberPairs[0];
        } else {
            // <configurationName_switchSN>
            fileName += "_";
            fileName += tc->serialNumberSwitches[0];
        }
        return saveFile(fileContent, fileName);
    }


}
