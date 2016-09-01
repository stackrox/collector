#ifndef _SYSDIG_
#define _SYSDIG_

#include <string>
#include <map>

#include "KafkaClient.h"

extern "C" {
    #include <sys/types.h>
    #include <inttypes.h>
}

namespace collector {

struct SysdigStats {
  uint64_t    nEvents;                // the number of kernel events
  uint64_t    nDrops;                 // the number of drops
  uint64_t    nPreemptions;           // the number of preemptions
  uint32_t    mLinePeriodicity;       // periodicity of M lines
  uint32_t    heartbeatDuration;      // heartbeat duration in millis
  uint64_t    nEventsDelta;           // events since last M line
  uint64_t    nDropsDelta;            // drops since last M line
  uint64_t    nPreemptionsDelta;      // preemptions since last M line
  uint32_t    nUpdates;               // number of topic map updates
  uint64_t    nFilteredEvents;        // events post chisel filter
  std::string nodeName;               // the name of this node (hostname)
};

class Sysdig {
    public:
    virtual ~Sysdig() {};

    virtual int init(std::string chiselName, std::string brokerList, std::string format,
                     bool useKafka, std::string defaultTopic, int snapLen) = 0;
    virtual bool ready() = 0;
    virtual void runForever() = 0;
    virtual void cleanup() = 0;

    virtual std::map<std::string, std::string> &containers() = 0;
    virtual bool commit() = 0;
    virtual bool stats(SysdigStats &s) = 0;
    virtual KafkaClient *getKafkaClient() = 0;
};

}   /* namespace collector */

#endif  /* _SYSDIG_ */

