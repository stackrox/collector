#ifndef _SYSDIG_SERVICE_H_
#define _SYSDIG_SERVICE_H_

#include <string>
#include <map>

#include "KafkaClient.h"
#include "Sysdig.h"

namespace collector {

class SysdigService : public Sysdig {
    public:
    SysdigService(bool &terminateFlag);
    virtual ~SysdigService();

    int init(std::string chiselName, std::string brokerList, std::string format,
             bool useKafka, std::string defaultTopic, int snapLen);
    bool ready();
    void runForever();
    void cleanup();

    void getSyscallIds(std::string syscall, std::vector<int>& ids);

    std::map<std::string, std::string> &containers();
    bool commit();
    bool stats(SysdigStats &s);
    KafkaClient *getKafkaClient();

    static std::string modulePath;
    static std::string moduleName;

    private:
    bool &terminate;
    std::map<std::string, std::string> containerMap;
    std::map<std::string, int> syscallsMap;
};

}   /* namespace collector */

#endif  /* _SYSDIG_SERVICE_H_ */

