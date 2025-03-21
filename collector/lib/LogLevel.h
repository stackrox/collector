#ifndef _LOG_LEVEL_H_
#define _LOG_LEVEL_H_

#include "CivetWrapper.h"

namespace collector {

class LogLevelHandler : public CivetWrapper {
 public:
  bool handlePost(CivetServer* server, struct mg_connection* conn) override;

  const std::string& GetBaseRoute() override {
    return kBaseRoute;
  }

 private:
  static const std::string kBaseRoute;
};

}  // namespace collector

#endif  // _LOG_LEVEL_H_
