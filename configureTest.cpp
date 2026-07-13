#include "configureTest.h"
#include "interfaceUtils.h"
#include <cctype>

namespace ConfigureTest {

    std::string getHumanReadableTimestamp() {
        time_t rawtime;
        struct tm *timeinfo;
        char buffer[80];

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",timeinfo);
        std::string str(buffer);
        return str;
    }

    // ---- small input helpers, so validation is consistent and mistake-proof ----

    // Strip leading/trailing whitespace (operators often add a stray space).
    static std::string trim(const std::string &s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if(a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static std::string toUpper(std::string s) {
        for(char &c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    }

    static bool allDigits(const std::string &s) {
        if(s.empty()) return false;
        for(char c : s) if(!std::isdigit((unsigned char)c)) return false;
        return true;
    }

    static bool allLetters(const std::string &s) {
        if(s.empty()) return false;
        for(char c : s) if(!std::isalpha((unsigned char)c)) return false;
        return true;
    }

    // Pair serial format: NNNNNNN-NNNNNNN  (7 digits, a dash, 7 digits).
    static bool validPairSerial(const std::string &s) {
        if(s.size() != 15 || s[7] != '-') return false;
        for(int i = 0; i < 15; i++) {
            if(i == 7) continue;
            if(!std::isdigit((unsigned char)s[i])) return false;
        }
        return true;
    }

    bool isBackCommand(std::string cin) {
        std::string t = toUpper(trim(cin));
        return t == "B";
    }

    void showStep(int stepN, testConfiguration *tc, std::string msg) {
        std::string userInput;
        switch(stepN) {
            case 0: { // Get operator initials
                std::string desc = msg.empty()
                    ? "Enter your initials for the test record.\nLetters only, 2-4 characters (e.g. GS)."
                    : msg;
                InterfaceUtils::createNewPage("Step 1/4", desc, "Operator initials:");
                InterfaceUtils::getStringFromCin(userInput);

                std::string initials = toUpper(trim(userInput));
                if(!allLetters(initials) || initials.size() < 2 || initials.size() > 4) {
                    showStep(stepN, tc, "Invalid initials. Enter 2-4 letters only (e.g. GS).");
                    break;
                }
                tc->operatorInitials = initials;
                showStep(stepN + 1, tc, "");
                break;
            }

            case 1: { // Get the number of pairs
                std::string title = "Step 2/4 - Operator: ";
                title += tc->operatorInitials;
                std::string desc = msg.empty()
                    ? "How many LAN-EX pairs are in this set?\nEnter a number from 1 to 8."
                    : msg;
                InterfaceUtils::createNewPage(title, desc, "Number of pairs (1-8, or 'b' to go back):");
                InterfaceUtils::getStringFromCin(userInput);

                std::string in = trim(userInput);
                if(isBackCommand(in)) {
                    showStep(stepN - 1, tc, "");
                    break;
                }
                if(!allDigits(in) || std::stoi(in) < 1 || std::stoi(in) > 8) {
                    showStep(stepN, tc, "Invalid number of pairs. Enter a whole number from 1 to 8.");
                    break;
                }

                tc->numOfPairs = std::stoi(in);
                tc->numOfSwitches = 0;   // switches are no longer part of a set
                // Human-readable config label shown on the monitor and in the report body.
                tc->configurationName = std::to_string(tc->numOfPairs) +
                    (tc->numOfPairs == 1 ? " pair" : " pairs");
                showStep(stepN + 1, tc, "");
                break;
            }

            case 2: { // Get the serial numbers of the LAN-EXs
                int currentPair = tc->serialNumberPairs.size();
                std::string title = "Step 3/4 - Operator: ";
                title += tc->operatorInitials;

                // Header reminds the operator how many pairs the set has, so a wrong count is
                // caught here rather than after the whole run.
                std::string header = "Pairs in this set: " + std::to_string(tc->numOfPairs) + "\n\n";

                std::string descMsg = header;
                for(int i = 0; i < currentPair; i++) {
                    descMsg += "Pair " + std::to_string(i + 1) + ": " + tc->serialNumberPairs[i] + "\n";
                }

                // Check if all serial numbers have been entered
                if(tc->serialNumberPairs.size() == tc->numOfPairs) {
                    // All input done, ask confirmation for serial number
                    std::string question = "Confirm these serial numbers? (y = yes, n = re-enter the last one)";
                    InterfaceUtils::createNewPage(title, descMsg, question);
                    InterfaceUtils::getStringFromCin(userInput);
                    std::string ans = toUpper(trim(userInput));
                    if(ans == "Y") {
                        showStep(stepN + 1, tc, "");
                    } else {
                        // Go back one entry so the operator can correct it
                        tc->serialNumberPairs.pop_back();
                        showStep(stepN, tc, "");
                    }
                    break;
                }

                std::string question = "Serials for pair " + std::to_string(currentPair + 1) +
                    " as NNNNNNN-NNNNNNN (or 'b' to go back):";

                // Show any error beneath the running list rather than replacing it.
                if(!msg.empty()) {
                    descMsg += "\n" + msg + "\n";
                }
                InterfaceUtils::createNewPage(title, descMsg, question);
                InterfaceUtils::getStringFromCin(userInput);
                std::string serial = trim(userInput);

                // Check for back
                if(isBackCommand(serial)) {
                    if(currentPair == 0) {
                        showStep(stepN - 1, tc, "");
                    } else {
                        tc->serialNumberPairs.pop_back();
                        showStep(stepN, tc, "");
                    }
                    break;
                }

                if(!validPairSerial(serial)) {
                    showStep(stepN, tc, "Invalid serial. Expected 7 digits, a dash, 7 digits "
                                        "(e.g. 1234567-7654321).");
                    break;
                }

                tc->serialNumberPairs.push_back(serial);
                showStep(stepN, tc, "");
                break;
            }

            case 3: { // Check Dip switches
                std::string title = "Step 4/4 - Operator: ";
                title += tc->operatorInitials;
                std::string desc = "Confirm the dip switches are set as shown:\n\n";
                desc += "        1   2   3   4\n";
                desc += "HEAD:  off-off-off-ON\n\n";
                desc += "        1   2   3   4\n";
                desc += "FIELD: ON -off-off-ON\n";
                if(!msg.empty()) {
                    desc += "\n" + msg + "\n";
                }

                InterfaceUtils::createNewPage(title, desc,
                    "Are the dip switches set correctly? (y = yes, or 'b' to go back)");
                InterfaceUtils::getStringFromCin(userInput);
                std::string ans = toUpper(trim(userInput));

                if(isBackCommand(ans)) {
                    showStep(stepN - 1, tc, "");
                } else if(ans == "Y") {
                    // Setup complete
                } else {
                    showStep(stepN, tc, "Please confirm the dip switch settings (y) before continuing.");
                }
                break;
            }
        }
    }

    testConfiguration startTestConfiguration() {
        testConfiguration tc;
        tc.timestamp = getHumanReadableTimestamp();
        showStep(0, &tc, "");
        return tc;
    }

}
