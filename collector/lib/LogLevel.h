#ifndef _LOG_LEVEL_H_
#define _LOG_LEVEL_H_

#include "civetweb/CivetServer.h"

namespace collector {

class LogLevel : public CivetHandler {
 public:
  bool handlePost(CivetServer* server, struct mg_connection* conn);
};

}  // namespace collector

#endif  // _LOG_LEVEL_H_
