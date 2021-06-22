#ifndef COLLECTOR_PROFILERHANDLER_H
#define COLLECTOR_PROFILERHANDLER_H

#include "civetweb/CivetServer.h"

namespace collector {

class ProfilerHandler : public CivetHandler {
 public:
  bool handleGet(CivetServer* server, struct mg_connection* conn);
};

}  // namespace collector

#endif  //COLLECTOR_PROFILERHANDLER_H
