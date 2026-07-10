#include "iperfExecutor.h"
#include <ncurses.h>
#include <sstream>
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
        outMsg.type = IPERF_ERROR;
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

            try {
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
            } catch (...) {
                outMsg.type = IPERF_ERROR;
            }

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

    // Extracts the Mbps value from an iperf3 line like "... 9.24 Mbits/sec ...".
    // Works for single-stream and bidir ([TX-C]/[RX-C]) lines. Returns -1 if none.
    static float extractBitrate(const std::string &line) {
        std::stringstream ss(line);
        std::string tok, prev;
        while(ss >> tok) {
            if(tok.find("bits/sec") != std::string::npos) {
                try {
                    float v = std::stof(prev);
                    if(tok[0] == 'G') v *= 1000.0f;
                    else if(tok[0] == 'K') v /= 1000.0f;
                    return v;
                } catch(...) { return -1.0f; }
            }
            prev = tok;
        }
        return -1.0f;
    }

    IperfResult startRemoteIperf3Client(int duration, LANEXTest::testData *td, int clientId, std::string remoteAddress,
        std::string serverAddress, int serverPort, bool isReversed, int bandwidthCap, bool bidir) {

        std::string cmd;
        // -n keeps ssh from reading the terminal's stdin, otherwise the ssh clients
        // swallow the operator's 'q' keypress instead of it reaching the app.
        cmd += "ssh -o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR -n ";
        cmd += "ubnt@";
        cmd += remoteAddress;
        cmd += " iperf3 -c ";
        cmd += serverAddress;
        cmd += " --port ";
        cmd += std::to_string(serverPort);
        cmd += " -t ";
        cmd += std::to_string(duration);
        if(bandwidthCap > 0) {           // Phase B caps each pair (e.g. -b 10M)
            cmd += " -b ";
            cmd += std::to_string(bandwidthCap);
            cmd += "M";
        }
        if(bidir) {                      // Phase B sends both directions at once
            cmd += " --bidir";
        } else if(isReversed) {
            cmd += " -R";
        }
        cmd += " 2>&1";

        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            // popen itself failed to launch the command — a setup error, not a link drop.
            return IPERF_RESULT_SETUP_ERROR;
        }

        td->testLogs[clientId] += "---\n";
        bool sawProgress = false;   // connection established and data actually flowed
        bool completed = false;     // reached a clean finish ("iperf Done.")

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            td->testLogs[clientId] += buffer.data();
            std::string raw = buffer.data();
            if(raw.find("iperf Done.") != std::string::npos) {
                completed = true;
            }

            if(bidir) {
                // Bidirectional interval lines are tagged with a role: [TX-C] / [RX-C].
                bool isTx = raw.find("[TX-C]") != std::string::npos;
                bool isRx = raw.find("[RX-C]") != std::string::npos;
                if(isTx || isRx) {
                    float rate = extractBitrate(raw);
                    if(rate >= 0) {
                        sawProgress = true;
                        if(isTx) td->currentTxRate[clientId] = rate;
                        if(isRx) td->currentRxRate[clientId] = rate;
                    }
                }
                continue;
            }

            IperfMessage msg = parseIperfBuffer(&buffer);
            // log progress
            if(msg.type == IPERF_PROGRESS_STATS) {
                sawProgress = true;
                if(isReversed) {
                    td->currentRxRate[clientId] = msg.bitrate;
                } else {
                    td->currentTxRate[clientId] = msg.bitrate;
                }
            } else if(msg.type == IPERF_COMPLETED_SENDER_STATS) {
                if(isReversed) td->averageRxRate[clientId] = msg.bitrate;
            } else if(msg.type == IPERF_COMPLETED_RECEIVER_STATS) {
                if(!isReversed) td->averageTxRate[clientId] = msg.bitrate;
            }
        }

        if(completed) {
            return IPERF_RESULT_COMPLETED;
        }

        // Did not finish cleanly. Distinguish a mid-stream connection drop (data flowed then
        // died) from a failure to even start (never connected). The engine acts on this
        // return value; the pair's rate slots are simply left unset.
        return sawProgress ? IPERF_RESULT_CONNECTION_DROP : IPERF_RESULT_SETUP_ERROR;
    }

    void startLocalIperf3Server(int serverPort, int timeoutSeconds) {
        std::string cmd;
        cmd += "timeout ";
        cmd += std::to_string(timeoutSeconds);
        cmd += " iperf3 -s --one-off --port ";
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

    void stopLocalIperf3Server(int serverPort) {
        std::string cmd;
        cmd += "pkill -f \"iperf3 -s --one-off --port ";
        cmd += std::to_string(serverPort);
        cmd += "\"";
        system(cmd.c_str());
    }

    void stopAllIperf3() {
        // Kill our local iperf3 servers and the local ssh processes running the remote
        // clients, closing every pipe. Patterns are matched to OUR exact invocations
        // (the one-off server and our specific ssh option string) to avoid killing
        // unrelated processes that merely mention iperf3.
        system("pkill -f 'iperf3 -s --one-off' 2>/dev/null");
        system("pkill -f 'ssh -o BatchMode=yes -o ConnectTimeout=5' 2>/dev/null");
    }

}
