#include "configureTest.h"
#include "interfaceUtils.h"

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

    bool isBackCommand(std::string cin) {
        if(cin.compare("b") == 0 || cin.compare("B") == 0) {
            return true;
        } else {
            return false;
        }
    }

    void showStep(int stepN, testConfiguration *tc, std::string msg) {
        std::string userInput;
        switch(stepN) {
            case 0: { // Get operator initials
                InterfaceUtils::createNewPage("Step 1/7", "", "Please enter the operator intials");
                InterfaceUtils::getStringFromCin(userInput);
                tc->operatorInitials = std::string(userInput);
                showStep(stepN + 1, tc, "");
                break;
            }

            case 1: { // Get name of configuration
                std::string title = "Step 2/7 - Operator: ";
                title += tc->operatorInitials;
                InterfaceUtils::createNewPage(title, msg, "Please enter the configuration name");
                InterfaceUtils::getStringFromCin(userInput);
                
                if(isBackCommand(userInput)) {
                    showStep(stepN - 1, tc, "");
                } else {
                    // Chech for valid configuration
                    if(userInput.find("HE-") == -1 || !isdigit(userInput.c_str()[3]) || !isdigit(userInput.c_str()[4])) {
                        // Invalid configuration number
                        showStep(stepN, tc, "The entered configuration number is invalid");
                        
                    } else {
                        tc->configurationName = std::string(userInput);

                        // Parse num of pairs and switches
                        std::string strNumOfPairs;
                        strNumOfPairs += userInput.c_str()[3];
                        std::string strNumOfSwitches;
                        strNumOfSwitches += userInput.c_str()[4];

                        tc->numOfPairs = std::stoi(strNumOfPairs);
                        tc->numOfSwitches = std::stoi(strNumOfSwitches);

                        // Check for max 8 units
                        if(tc->numOfPairs < 1 || tc->numOfPairs > 8) {
                            showStep(stepN, tc, "The entered configuration number is invalid (Minimum 1 pair, max 8 pairs)");
                            break;
                        }
                        

                        // Check for no switches when 1 pair
                        if(tc->numOfPairs == 1 && tc->numOfSwitches != 0) {
                            showStep(stepN, tc, "The entered configuration number is invalid (1 pair set must not have any switches)");
                            break;
                        }

                        showStep(stepN + 1, tc, "");
                    }
                }
                break;   
            }

            case 2: { // Get the serial numbers of the LAN-EXs
                int currentPair = tc->serialNumberPairs.size();
                std::string title = "Step 3/7 - Operator: ";
                title += tc->operatorInitials;

                // Get all serial numbers
                std::string descMsg;
                for(int i = 0; i < currentPair; i++) {
                    descMsg += "Pair ";
                    descMsg += std::to_string(i + 1);
                    descMsg += ": ";
                    descMsg += tc->serialNumberPairs[i];
                    descMsg += "\n";
                }

                // Check if all serial numbers have been entered
                if(tc->serialNumberPairs.size() == tc->numOfPairs) {
                    // All input done, ask confirmation for serial number
                    std::string question = "Do you confirm the listed serial numbers? (y/n)";
                    InterfaceUtils::createNewPage(title, descMsg, question);
                    InterfaceUtils::getStringFromCin(userInput);
                    if(userInput.compare("y") == 0 || userInput.compare("Y") == 0) {
                        showStep(stepN + 1, tc, "");
                    } else {
                        // Go back
                        tc->serialNumberPairs.pop_back();
                        showStep(stepN, tc, "");
                    }
                    break;
                }

                std::string question = "Please enter the serial numbers for the pair ";
                question += std::to_string(currentPair + 1);
                question += ": (serialNumber1-serialNumber2)";
                
                // Check for error msg
                if (msg.compare("") != 0) {
                    descMsg = msg;
                }
                InterfaceUtils::createNewPage(title, descMsg, question);

                InterfaceUtils::getStringFromCin(userInput);
                
                // Check for back
                if(isBackCommand(userInput)) {
                    if(currentPair == 0) {
                        showStep(stepN - 1, tc, "");
                    } else {
                        tc->serialNumberPairs.pop_back();
                        showStep(stepN, tc, "");
                    }
                    break;
                }

                // Check input val: 7 serial num digits + 1 spacer + 7 serial num digits
                if(userInput.size() != 15 || userInput.find("-") == -1) { 
                    showStep(stepN, tc, "Invalid serial number entered: syntax (serialNumber1-serialNumber2)");
                    break;    
                }

                tc->serialNumberPairs.push_back(userInput);
                showStep(stepN, tc, "");

                break;
            }

            case 3: { // Get the serial numbers of the switches
                // No switches are needed if just 1 pair, skip
                if(tc->numOfPairs == 1) {
                    showStep(stepN + 1, tc, "");
                    break;
                }
                int currentSwitch = tc->serialNumberSwitches.size();
                std::string title = "Step 4/7 - Operator: ";
                title += tc->operatorInitials;

                // Get all serial numbers
                std::string descMsg;
                for(int i = 0; i < currentSwitch; i++) {
                    descMsg += "Switch ";
                    descMsg += std::to_string(i + 1);
                    descMsg += ": ";
                    descMsg += tc->serialNumberSwitches[i];
                    descMsg += "\n";
                }

                // Check if all serial numbers have been entered
                if(tc->serialNumberSwitches.size() == tc->numOfSwitches) {
                    // All input done, ask confirmation for serial number
                    std::string question = "Do you confirm the listed serial numbers? (y/n)";
                    InterfaceUtils::createNewPage(title, descMsg, question);
                    InterfaceUtils::getStringFromCin(userInput);
                    if(userInput.compare("y") == 0 || userInput.compare("Y") == 0) {
                        showStep(stepN + 1, tc, "");
                    } else {
                        // Go back
                        tc->serialNumberSwitches.pop_back();
                        showStep(stepN, tc, "");
                    }
                    break;
                }

                std::string question = "Please enter the serial numbers for the switch ";
                question += std::to_string(currentSwitch + 1);
                question += ":";
                
                // Check for error msg
                if (msg.compare("") != 0) {
                    descMsg = msg;
                }

                InterfaceUtils::createNewPage(title, descMsg, question);

                InterfaceUtils::getStringFromCin(userInput);
                
                // Check for back
                if(isBackCommand(userInput)) {
                    if(currentSwitch == 0) {
                        showStep(stepN - 1, tc, "");
                    } else {
                        tc->serialNumberSwitches.pop_back();
                        showStep(stepN, tc, "");
                    }
                    break;
                }

                // Check input val: 6 serial num
                if(userInput.size() != 6) { 
                    showStep(stepN, tc, "Invalid serial number entered");
                    break;    
                }

                tc->serialNumberSwitches.push_back(userInput);
                showStep(stepN, tc, "");

                break;
            }

            case 4: { // Check green sticker
                // No switches are needed if just 1 pair, skip
                if(tc->numOfPairs == 1) {
                    showStep(stepN + 1, tc, "");
                    break;
                }
                
                std::string title = "Step 5/7 - Operator: ";
                title += tc->operatorInitials;
                InterfaceUtils::createNewPage(title, msg, "Do you confirm that a green sticker is present on the switches? (y)");
                InterfaceUtils::getStringFromCin(userInput);
                
                if(isBackCommand(userInput)) {
                    showStep(stepN - 1, tc, "");
                } else {
                    if(userInput.compare("y") == 0 || userInput.compare("Y") == 0) {
                        // Invalid configuration number
                        showStep(stepN + 1, tc, "");
                    } else {
                        showStep(stepN, tc, "");
                    }
                }
                break;   
            }

            case 5: { // Check Dip switches
                std::string title = "Step 6/7 - Operator: ";
                title += tc->operatorInitials;
                msg = "Do you confirm that the following dip switches are set?\n";
                msg += "        1   2  3   4\n";
                msg += "HEAD:  off-ON-off-off\n\n";
                msg += "        1   2  3   4\n";
                msg += "FIELD: ON-off-off-off";

                InterfaceUtils::createNewPage(title, msg, "(y)?");
                InterfaceUtils::getStringFromCin(userInput);
                
                if(isBackCommand(userInput)) {
                    showStep(stepN - 1, tc, "");
                } else {
                    if(userInput.compare("y") == 0 || userInput.compare("Y") == 0) {
                        // Invalid configuration number
                        showStep(stepN + 1, tc, "");
                    } else {
                        showStep(stepN, tc, "");
                    }
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