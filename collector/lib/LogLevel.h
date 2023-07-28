#ifndef _LOG_LEVEL_H_
#define _LOG_LEVEL_H_

#include "CivetServer.h"

namespace collector {

class LogLevelHandler : public CivetHandler {
 public:
  bool handlePost(CivetServer* server, struct mg_connection* conn);
};

}  // namespace collector

#endif  // _LOG_LEVEL_H_
