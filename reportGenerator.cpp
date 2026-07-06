#include "reportGenerator.h"

namespace ReportGenerator {
    

    bool saveFile(std::string fileContent, std::string fileName) {
        std::ofstream file;
        std::string outLocation = "reports/";
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
