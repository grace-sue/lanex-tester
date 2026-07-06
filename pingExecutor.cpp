#include "pingExecutor.h"

namespace PingExecutor {
    bool pingDevice(std::string ip) {
        std::string cmd;
        cmd += "ping -c 1 -W2 ";
        cmd += ip;

        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            std::cout << "popen() failed!";
        }
        // Read line by line command output
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            std::string currLine = buffer.data();
            
            // Parse final line to get the result
            if(currLine.find("1 packets transmitted") != -1) {
                if(currLine.find("100\% packet loss") != -1) {
                    return false; // packet was lost
                } else {
                    return true; // packet was received
                }
            }
        }

        return true;
    }

    // -1 all OK!
    // all other values: pair that had a problem
    int pingAll(int numOfPairs, std::vector<std::string> *ips) {
        for(int i = 0; i < numOfPairs; i++) {
            if(!pingDevice((*ips)[i])) {
                return i; // Return the number of the pair that's not working
            }
        }

        return -1; // Everything is working
    } 

    void pingAllTestWindow(int numOfPairs, std::vector<std::string> *ips) {
        while (true) {
            InterfaceUtils::createNewPage("Preliminary Test", "Please wait until the pre-test checks are completed", "");
            int pingTestResult = PingExecutor::pingAll(numOfPairs, ips);
            if(pingTestResult != -1) {
                // A pair was offline
                std::string msg = "Pair ";
                msg += std::to_string(pingTestResult + 1);
                msg += " could not pass the preliminary test\n\nPlease check its connections";

                InterfaceUtils::createNewPage("Preliminary Test Failed", msg, "Press any key to try again");
                std::string tempStr;
                InterfaceUtils::getStringFromCin(tempStr);
            } else {
                break; // Continue to next step
            }
        }
    }
}