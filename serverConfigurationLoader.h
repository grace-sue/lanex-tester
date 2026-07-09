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
        int phaseDuration;   // seconds per direction, Phase A (throughput)
        int soakDuration;    // seconds, Phase B (capped soak)
        int soakCap;         // Mbps per pair cap, Phase B
        int maxConnDrops;    // connection drops allowed before a pair FAILs (0 = any drop fails)
        int retries;         // re-attempts for a measurement that couldn't start
    };
    
    ServerConfiguration loadConfiguration();
}
#endif