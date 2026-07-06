#include "iperfExecutor.h"
#include <ncurses.h>
namespace IperfExecutor {
    // Helper functions
    const std::string WHITESPACE = " \n\r\t\f\v";
    
    std::string ltrim(const std::string &s) {
        size_t start = s.find_first_not_of(WHITESPACE);
        return (start == std::string::npos) ? "" : s.substr(start);
    }
    
    std::string rtrim(const std::string &s) {
        size_t end = s.find_last_not_of(WHITESPACE);
        return (end == std::string::npos) ? "" : s.substr(0, end + 1);
    }

    struct IperfMessage {
        IperfMessageType type;
        std::string interval;
        std::string transferUnit;
        float transfer;
        float bitrate;
        std::string bitrateUnit;
    };

    bool checkFirstCharacters(std::array<char, 128> *buffer, const char *key) {
        for(int i = 0; i < strlen(key); i++) {
            if(buffer->data()[i] != key[i]) {
                return false; // Key is not the same
            }
        }
        return true; // Key is the same
    }

    IperfMessage parseIperfBuffer(std::array<char, 128> *buffer) {
        IperfMessage outMsg;
        std::string cmdStdoutLine = buffer->data();

        // Stats msg flag
        if(checkFirstCharacters(buffer, "[  5]") || checkFirstCharacters(buffer, "[  4]")) {
            // Check for first connection stats
            if(cmdStdoutLine.find("connected to") != -1) {
                outMsg.type = IPERF_IGNORE;
                return outMsg;
            }

            // Remove flag
            cmdStdoutLine.erase(0, 8);
            
            // Check if ending stats
            if(cmdStdoutLine.find("sender") != -1) {
                outMsg.type = IPERF_COMPLETED_SENDER_STATS;
            } else if(cmdStdoutLine.find("receiver") != -1) {
                outMsg.type = IPERF_COMPLETED_RECEIVER_STATS;
            } else {
                outMsg.type = IPERF_PROGRESS_STATS;
            }

            // Interval
            int nextDelimiter = cmdStdoutLine.find("  ");
            outMsg.interval = cmdStdoutLine.substr(0, nextDelimiter); 

            // Transfer
            nextDelimiter = cmdStdoutLine.find("sec") + 3;
            cmdStdoutLine.erase(0, nextDelimiter);
            cmdStdoutLine = ltrim(cmdStdoutLine);
            nextDelimiter = cmdStdoutLine.find(" ");
            outMsg.transfer = std::stof(cmdStdoutLine.substr(0, nextDelimiter));

            // Transfer unit
            cmdStdoutLine.erase(0, nextDelimiter + 1);
            nextDelimiter = cmdStdoutLine.find(" ");
            outMsg.transferUnit = cmdStdoutLine.substr(0, nextDelimiter);
            
            // Bitrate
            cmdStdoutLine.erase(0, nextDelimiter);
            cmdStdoutLine = ltrim(cmdStdoutLine);
            nextDelimiter = cmdStdoutLine.find(" ");
            outMsg.bitrate = std::stof(cmdStdoutLine.substr(0, nextDelimiter));
            
            // Bitrate unit
            cmdStdoutLine.erase(0, nextDelimiter + 1);
            nextDelimiter = cmdStdoutLine.find(" ");
            outMsg.bitrateUnit = cmdStdoutLine.substr(0, nextDelimiter);

        } else if(checkFirstCharacters(buffer, "Connecting to host") || 
            checkFirstCharacters(buffer, "[ ID]") ||
            checkFirstCharacters(buffer, "- - - - - - - - - - - - - - -") || 
            checkFirstCharacters(buffer, "iperf Done.") ||
            checkFirstCharacters(buffer, "Reverse mode, remote") ||
            checkFirstCharacters(buffer, "\r") ||
            checkFirstCharacters(buffer, "\n")
            ) {
                // These messages can be ignored
                outMsg.type = IPERF_IGNORE;
        } else { 
            // Message not correctly parsed
            outMsg.type = IPERF_ERROR;
        }

        return outMsg;
    }

    void startRemoteIperf3Client(int duration, LANEXTest::testData *td, int clientId, std::string remoteAddress, 
        std::string serverAddress, int serverPort, bool isReversed) {
        
        std::string cmd;
        cmd += "ssh ";
        cmd += "ubnt@";
        cmd += remoteAddress;
        cmd += " -o LogLevel=QUIET -t iperf3 -c ";
        cmd += serverAddress;
        cmd += " --port ";
        cmd += std::to_string(serverPort);
        cmd += " -t ";
        cmd += std::to_string(duration);
        // cmd += " --connect-timeout 1000";
        // cmd += " -b 5m";

        if(isReversed) {
            cmd += " -R";
        }
        cmd += " 2>&1";

        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            std::cout << "popen() failed!";
            td->averageRxRate[clientId] = -2;
            td->averageTxRate[clientId] = -2;
            return;
        }

        td->testLogs[clientId] += "---\n";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            td->testLogs[clientId] += buffer.data();
            IperfMessage msg = parseIperfBuffer(&buffer);
            // log progress
            if(msg.type == IPERF_PROGRESS_STATS) {
                if(isReversed) {
                    td->currentRxRate[clientId] = msg.bitrate;
                } else {
                    td->currentTxRate[clientId] = msg.bitrate;
                }
            } else if(msg.type == IPERF_COMPLETED_SENDER_STATS && isReversed) {
                td->averageRxRate[clientId] = msg.bitrate;
            } else if(msg.type == IPERF_COMPLETED_RECEIVER_STATS && !isReversed) {
                td->averageTxRate[clientId] = msg.bitrate;
            } else if(msg.type == IPERF_ERROR) {
                // std::cout << "HERE!";
                // move(5,0);
                // printw((std::string(buffer.data())).c_str());
                // move(6,0);
                // printw(std::to_string((std::string(buffer.data())).length()).c_str());
                td->averageRxRate[clientId] = -2;
                td->averageTxRate[clientId] = -2;
            }
        }
    }

    void startLocalIperf3Server(int serverPort) {
        std::string cmd;
        cmd += "iperf3 -s --one-off --port ";
        cmd += std::to_string(serverPort);
        cmd += " 2>&1";

        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            std::cout << "popen() failed!";
            return;
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
            IperfMessage msg = parseIperfBuffer(&buffer);
        }
    }

}
