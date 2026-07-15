#include "iperfExecutor.h"
#include <ncurses.h>
#include <sstream>
namespace IperfExecutor {
    // Helper functions
    const std::string WHITESPACE = " \n\r\t\f\v";

    // Whether the remote iperf3 accepts --forceflush (3.1+). Set once by
    // detectRemoteCapabilities() before the test loop, read during it. Default off so an
    // undetected / old iperf3 is never handed a flag it would reject.
    static bool g_useForceflush = false;
    
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

    // True for an iperf3 per-stream line like "[  5] ...", "[  4] ...", "[ 10] ..." — any
    // numeric stream id — but not the header "[ ID]" or a "[SUM]" line. Keeps interval
    // parsing independent of which stream id a given iperf3 version happens to use.
    bool isStreamStatLine(const std::string &line) {
        if(line.empty() || line[0] != '[') return false;
        size_t i = 1;
        while(i < line.size() && line[i] == ' ') i++;
        size_t digitStart = i;
        while(i < line.size() && line[i] >= '0' && line[i] <= '9') i++;
        if(i == digitStart) return false;             // no digits (e.g. "[ ID]", "[SUM]")
        return i < line.size() && line[i] == ']';
    }

    IperfMessage parseIperfBuffer(std::array<char, 128> *buffer) {
        IperfMessage outMsg;
        outMsg.type = IPERF_ERROR;
        std::string cmdStdoutLine = buffer->data();

        // Stats msg flag
        if(isStreamStatLine(cmdStdoutLine)) {
            // Check for first connection stats
            if(cmdStdoutLine.find("connected to") != -1) {
                outMsg.type = IPERF_IGNORE;
                return outMsg;
            }

            // Remove the "[  N]" stream tag (id width varies by version, so strip up to ']')
            cmdStdoutLine.erase(0, cmdStdoutLine.find(']') + 1);
            cmdStdoutLine = ltrim(cmdStdoutLine);

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

    bool remoteSupportsForceflush() {
        return g_useForceflush;
    }

    // Ask the remote iperf3 its version and cache whether it supports --forceflush (3.1+).
    // Run once, before the (multi-threaded) test loop, so the read is race-free afterwards.
    void detectRemoteCapabilities(std::string remoteAddress) {
        g_useForceflush = false;   // safe default: an old iperf3 rejects the flag
        std::string cmd =
            "ssh -o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new "
            "-o LogLevel=ERROR -n ubnt@" + remoteAddress + " iperf3 --version 2>&1";

        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if(!pipe) return;

        while(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            std::string line = buffer.data();
            size_t p = line.find("iperf ");           // version line, e.g. "iperf 3.6 (cJSON ...)"
            if(p == std::string::npos) continue;
            std::string ver = line.substr(p + 6);      // "3.6 (cJSON ...)"
            try {
                int major = std::stoi(ver);
                int minor = 0;
                size_t dot = ver.find('.');
                if(dot != std::string::npos) minor = std::stoi(ver.substr(dot + 1));
                g_useForceflush = (major > 3) || (major == 3 && minor >= 1);
            } catch(...) { /* leave default off */ }
            break;
        }
    }

    IperfResult startRemoteIperf3Client(int duration, LANEXTest::testData *td, int clientId, std::string remoteAddress,
        std::string serverAddress, int serverPort, bool isReversed, int bandwidthCap, bool bidir) {

        std::string cmd;
        // Hard cap on the whole measurement: if the link (and thus the ssh session) dies,
        // this guarantees the command is killed instead of blocking our read forever.
        cmd += "timeout ";
        cmd += std::to_string(duration + 15);
        cmd += " ";
        // -n keeps ssh from reading the terminal's stdin, otherwise the ssh clients
        // swallow the operator's 'q' keypress instead of it reaching the app.
        // ServerAlive* makes ssh detect a dead session (~6s) and exit, so a connection
        // drop ends the read (EOF) instead of hanging. ConnectTimeout only covers connecting.
        cmd += "ssh -o BatchMode=yes -o ConnectTimeout=5 -o ServerAliveInterval=3 -o ServerAliveCountMax=2 -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR -n ";
        cmd += "ubnt@";
        cmd += remoteAddress;
        cmd += " iperf3 -c ";
        cmd += serverAddress;
        cmd += " --port ";
        cmd += std::to_string(serverPort);
        cmd += " -t ";
        cmd += std::to_string(duration);
        // Force iperf3 to flush stdout after every interval. Over ssh (no TTY) iperf3's
        // stdout is a pipe, which glibc block-buffers — without this, newer iperf3 (3.x)
        // holds all interval lines until the test ends, so the live rates never update.
        // Only added when the remote iperf3 is known to support it (3.1+), so an older
        // iperf3 that would reject the flag still runs. See detectRemoteCapabilities().
        if(g_useForceflush) {
            cmd += " --forceflush";
        }
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
        bool hadStall = false;      // an interval moved 0 bytes (a momentary link drop)

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
                        if(rate <= 0.0f) hadStall = true; // 0-byte interval = drop
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
                if(msg.bitrate <= 0.0f) hadStall = true; // 0-byte interval = drop
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

        if(hadStall) {
            // An interval moved 0 bytes: the link stalled mid-run. Count it as a drop even
            // if iperf3 recovered and printed "iperf Done.".
            return IPERF_RESULT_CONNECTION_DROP;
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
