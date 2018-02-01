#ifndef _SYSDIG_API_H_
#define _SYSDIG_API_H_

#include <cstdint>
#include <string>

extern "C" {

typedef struct {
    uint64_t    nEvents;                // the number of kernel events
    uint64_t    nDrops;                 // the number of drops
    uint64_t    nPreemptions;           // the number of preemptions
    uint64_t    nFilteredEvents;        // events post chisel filter
} sysdigStatsT;

int sysdigInitialize(std::string chiselName, std::string brokerList, std::string format, bool useKafka,
                     std::string defaultTopic, std::string networkTopic, std::string processTopic, std::string fileTopic,
                     std::string processSyscalls, int snapLen);
void sysdigCleanup();
void sysdigStartProduction(bool& isInterrupted);
bool sysdigGetSysdigStats(sysdigStatsT& sysdigStats);
bool isSysdigInitialized();
const std::string& sysdigGetNodeName();

}

#endif
