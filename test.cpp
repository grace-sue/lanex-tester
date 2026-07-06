#include "test.h"
#include <unistd.h>
#include <thread>


namespace LANEXTest {
    int startTestRound(testData *testData, ConfigureTest::testConfiguration *tc, 
        ServerConfigurationLoader::ServerConfiguration *serverConf, bool flipped) {
        int numOfTestPairs = tc->numOfPairs;

        // Start servers
        int portStart = 5201;
        std::thread serverThreads[numOfTestPairs];
        for(int i = 0; i < numOfTestPairs; i++) {
            serverThreads[i] = std::thread(IperfExecutor::startLocalIperf3Server, portStart + i);
        }

        // Start clients
        std::thread clientThreads[numOfTestPairs];
        for(int i = 0; i < numOfTestPairs; i++) {
            if(i > 3) { // If more than 3 tests are running, flip its direction
                if(flipped) {
                    clientThreads[i] = std::thread(IperfExecutor::startRemoteIperf3Client, serverConf->duration, testData,
                                            i, (*serverConf).clientIps[i], (*serverConf).serverIps[i], portStart + i, true);
                } else {
                    clientThreads[i] = std::thread(IperfExecutor::startRemoteIperf3Client, serverConf->duration, testData,
                                            i, (*serverConf).clientIps[i], (*serverConf).serverIps[i], portStart + i, false);
                }
            } else {
                if(flipped) {
                    clientThreads[i] = std::thread(IperfExecutor::startRemoteIperf3Client, serverConf->duration, testData,
                                            i, (*serverConf).clientIps[i], (*serverConf).serverIps[i], portStart + i, false);
                } else {
                    clientThreads[i] = std::thread(IperfExecutor::startRemoteIperf3Client, serverConf->duration, testData,
                                            i, (*serverConf).clientIps[i], (*serverConf).serverIps[i], portStart + i, true);
                }
            }
            
        } 

        int errorWhileTesting = -1;
        int timeRemaining = serverConf->duration;
        if(!flipped) {
            timeRemaining += serverConf->duration;
        }

        // Update transfer rates every second
        while(true) {
            // Update progress
            std::string timeProgress = std::to_string(timeRemaining--);
            timeProgress += " seconds remaining";
            InterfaceUtils::updateProgress(timeProgress);

            // Check if all clients have ended (they have an average transfer rate)
            int finishedTests = 0;
            for(int i = 0; i < numOfTestPairs; i++) {
                if(testData->averageRxRate[i] != -1) { 
                    finishedTests++;
                } 
                if(testData->averageTxRate[i] != -1) {
                    finishedTests++;
                }  

                // Check for error 
                if(testData->averageRxRate[i] == -2 || testData->averageTxRate[i] == -2) {
                    errorWhileTesting = i;
                }
            }
            
            if(!flipped) {
                if(finishedTests == numOfTestPairs || errorWhileTesting != -1) {
                    break; // All threads have ended or error, stop
                }
            } else { // Since data is shared with previous test, all -1 have to be cleared from stats to pass
                if(finishedTests == numOfTestPairs * 2 || errorWhileTesting != -1) {
                    break; // All threads have ended or error, stop
                }
            }
            
            usleep(1 * 1000000); // Wait 1 second
            
            // Update stats
            for(int i = 0; i < numOfTestPairs; i++) {
                if(testData->currentRxRate[i] != -1) {
                    InterfaceUtils::updateRxOfPair(i, std::to_string(testData->currentRxRate[i]));
                } 
                if(testData->currentTxRate[i] != -1) {
                    InterfaceUtils::updateTxOfPair(i, std::to_string(testData->currentTxRate[i]));
                }  
            }
            

        }

        // Wait for servers and clients
        for(int i = 0; i < numOfTestPairs; i++) {
            // TODO: Kill threads if there's an error
            serverThreads[i].join();
            clientThreads[i].join();
        } 

        return errorWhileTesting;
    }

    bool startTest(ConfigureTest::testConfiguration *tc, 
                ServerConfigurationLoader::ServerConfiguration *serverConf,
                testData &testData) {
        while (true) { // Loop to repeat the test

        
            // Initialize data in TestData
            for(int i = 0; i < 8; i++) {
                testData.averageRxRate[i] = -1;
                testData.averageTxRate[i] = -1;
                testData.currentRxRate[i] = -1;
                testData.currentTxRate[i] = -1;
            }

            // Get num of pairs to test
            int numOfTestPairs = tc->numOfPairs;

            // Show screen
            InterfaceUtils::createNewTestMonitorPage(tc);

            // Start test in one direction 
            int errCode = startTestRound(&testData, tc, serverConf, false);
            if(errCode != -1) {
                std::string msg = "An error has occoured while testing pair ";
                msg += std::to_string(errCode + 1);
                InterfaceUtils::createNewPage("Error while testing", msg, 
                                                "Do you want to restart the test? (y/n)");
                char userInput = getch();
                if(userInput == 'y' || userInput == 'Y') {
                    // Ping test
                    PingExecutor::pingAllTestWindow(numOfTestPairs, &serverConf->clientIps);
                    continue;
                } else {
                    return false;
                }
            }

            errCode = startTestRound(&testData, tc, serverConf, true);
            if(errCode != -1) {
                std::string msg = "An error has occoured while testing pair ";
                msg += std::to_string(errCode + 1);
                InterfaceUtils::createNewPage("Error while testing", msg, 
                                                "Do you want to restart the test? (y/n)");
                
                char userInput = getch();
                if(userInput == 'y' || userInput == 'Y') {
                    // Ping test
                    PingExecutor::pingAllTestWindow(numOfTestPairs, &serverConf->clientIps);
                    continue;
                } else {
                    return false;
                }
            } else {
                
            }

            // Check averages
            int passed = -1; // If -1 passed, if not it holds the causing pair
            for(int i = 0; i < numOfTestPairs; i++) {
                if(testData.averageRxRate[i] <= serverConf->rxTargetSpeed ||
                    testData.averageTxRate[i] <= serverConf->txTargetSpeed) {
                    passed = i;
                    break;
                }
            }

            if(passed != -1) {
                std::string msg = "Pair ";
                msg += std::to_string(passed + 1);
                msg += " didn't hit the target transfer rate";
                InterfaceUtils::createNewPage("Error while testing", msg, 
                                                "Do you want to restart the test? (y/n)");
                
                char userInput = getch();
                if(userInput == 'y' || userInput == 'Y') {
                    // Ping test
                    PingExecutor::pingAllTestWindow(numOfTestPairs, &serverConf->clientIps);
                    continue;
                } else {
                    return false;
                }
            } else {
                InterfaceUtils::createNewPage("result: PASSED!", "PASSED!", "Press any key to save the report");
                getch();
                return true;
            }

        }
    }
}
