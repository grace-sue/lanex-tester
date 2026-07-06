#include "serverConfigurationLoader.h"
#include <ncurses.h>
namespace ServerConfigurationLoader {
    

    std::vector<std::string> openFile(const char *path) {
        std::vector<std::string> outVect;
        
        std::string line;
        std::ifstream file(path);
        if (file.is_open()){
            while (std::getline (file,line)) {
                outVect.push_back(line);
            }
            file.close();
        }

        return outVect;
    }

    ServerConfiguration loadConfiguration() {
        ServerConfiguration outConf;
        
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
            }
        }

        outConf.serverIps = serverIps;
        outConf.clientIps = clientIps;

        return outConf;
    }  
}