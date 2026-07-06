#ifndef SERVER_CONFIGURATION_LOADER
#define SERVER_CONFIGURATION_LOADER
#include <vector>
#include <string>
#include <fstream>
namespace ServerConfigurationLoader {
    struct ServerConfiguration {
        std::vector<std::string> serverIps;
        std::vector<std::string> clientIps;
        float txTargetSpeed;
        float rxTargetSpeed;
        int duration;
    };
    
    ServerConfiguration loadConfiguration();
}
#endif