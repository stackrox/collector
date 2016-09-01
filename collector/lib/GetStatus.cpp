#include <map>
#include <string>

#include <json/json.h>

#include "ContainersMessage.h"
#include "GetStatus.h"
#include "Request.h"
#include "Response.h"
#include "Sysdig.h"

namespace collector {

GetStatus::GetStatus(Sysdig *sysdigService)
    : sysdig(sysdigService)
{
}

GetStatus::~GetStatus()
{
}

void
GetStatus::handleRequest(const Request *request, Response &response)
{
    using namespace std;

    Json::Value status(Json::objectValue);
    status["status"] = sysdig->ready() ? "ok" : "not ready";

    if (sysdig->ready()) {
        status["containers"] = ContainersMessage(sysdig->containers()).toJSON();
    } else {
        status["containers"] = Json::Value(Json::objectValue);
    }

    if (sysdig->ready()) {
        SysdigStats stats;
        if (!sysdig->stats(stats)) {
            status["sysdig"] = Json::Value(Json::objectValue);
            status["sysdig"]["status"] = "stats unavailable";
        } else {
            status["sysdig"] = Json::Value(Json::objectValue);
            status["sysdig"]["status"] = "ok";
            status["sysdig"]["node"] = stats.nodeName;
            status["sysdig"]["events"] = Json::UInt64(stats.nEvents);
            status["sysdig"]["drops"] = Json::UInt64(stats.nDrops);
            status["sysdig"]["preemptions"] = Json::UInt64(stats.nPreemptions);
            status["sysdig"]["m_line_periodocity"] = Json::UInt(stats.mLinePeriodicity);
            status["sysdig"]["heartbeat_duration"] = Json::UInt(stats.heartbeatDuration);
            status["sysdig"]["events_delta"] = Json::UInt64(stats.nEventsDelta);
            status["sysdig"]["drops_delta"] = Json::UInt64(stats.nDropsDelta);
            status["sysdig"]["preemptions_delta"] = Json::UInt64(stats.nPreemptionsDelta);
            status["sysdig"]["updates"] = Json::UInt(stats.nUpdates);
            status["sysdig"]["filtered_events"] = Json::UInt64(stats.nFilteredEvents);
        }
    }

    response.statusCode = 200;
    response.body = status;
}

}   /* namespace collector */

