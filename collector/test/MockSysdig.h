#ifndef _MOCK_SYSDIG_
#define _MOCK_SYSDIG_

#include <string>
#include <map>

#include "Sysdig.h"

#include "gmock/gmock.h"

namespace collector {

class MockSysdig : public Sysdig {
    public:
    MOCK_METHOD6(init, int(std::string chiselName, std::string brokerList, std::string format,
                           bool useKafka, std::string defaultTopic, int snapLen));
    MOCK_METHOD0(ready, bool());
    MOCK_METHOD0(runForever, void());
    MOCK_METHOD0(cleanup, void());

    MOCK_METHOD0(containers, std::map<std::string, std::string> &());
    MOCK_METHOD0(commit, bool());
    MOCK_METHOD1(stats, bool(SysdigStats &s));
    MOCK_METHOD0(getKafkaClient, KafkaClient*());
};

}   /* namespace collector */

#endif  /* _MOCK_SYSDIG_ */

