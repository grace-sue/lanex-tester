#include "serverConfigurationLoader.h"
#include <ncurses.h>
namespace ServerConfigurationLoader {
    

    std::vector<std::string> openFile(const char *path) {
        std::vector<std::string> outVect;
        
        std::string line;
        std::ifstream file(path);
        if (file.is_open()){
            while (std::getline (file,line)) {
                if(!line.empty() && line[line.size() - 1] == '\r') {
                    line.erase(line.size() - 1);
                }
                if(!line.empty()) {
                    outVect.push_back(line);
                }
            }
            file.close();
        }

        return outVect;
    }

    ServerConfiguration loadConfiguration() {
        ServerConfiguration outConf;
        outConf.txTargetSpeed = 0;
        outConf.rxTargetSpeed = 0;
        outConf.duration = 30;
        outConf.phaseDuration = 5;
        outConf.soakDuration = 30;
        outConf.soakCap = 10;
        outConf.retries = 3;
        
        // Load server ips
        std::vector<std::string> serverIps = openFile("config/serverIps.conf");
        
        // Load client ips
        std::vector<std::string> clientIps = openFile("config/clientIps.conf");

        // Load target transfer rate
        std::vector<std::string> targetTransferRateFile = openFile("config/targetBandwidth.conf");
        for(int i = 0; i < targetTransferRateFile.size(); i++) {
            std::string currLine = targetTransferRateFile[i];
            if(currLine.find("RX:") != -1){
                currLine.erase(0, 3);
                outConf.rxTargetSpeed = std::stof(currLine);
            } else if(currLine.find("TX:") != -1){
                currLine.erase(0, 3);
                outConf.txTargetSpeed = std::stof(currLine);
            } else if(currLine.find("duration:") != -1){
                currLine.erase(0, 9);
                outConf.duration = std::stoi(currLine);
            } else if(currLine.find("phaseDuration:") != std::string::npos){
                outConf.phaseDuration = std::stoi(currLine.substr(currLine.find(":") + 1));
            } else if(currLine.find("soakDuration:") != std::string::npos){
                outConf.soakDuration = std::stoi(currLine.substr(currLine.find(":") + 1));
            } else if(currLine.find("soakCap:") != std::string::npos){
                outConf.soakCap = std::stoi(currLine.substr(currLine.find(":") + 1));
            } else if(currLine.find("retries:") != std::string::npos){
                outConf.retries = std::stoi(currLine.substr(currLine.find(":") + 1));
            }
        }

        outConf.serverIps = serverIps;
        outConf.clientIps = clientIps;

        return outConf;
    }  
}
