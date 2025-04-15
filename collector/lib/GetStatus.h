#ifndef _GET_STATUS_H_
#define _GET_STATUS_H_

#include <string>

#include "CivetWrapper.h"
#include "system-inspector/SystemInspector.h"

namespace collector {

class GetStatus : public CivetWrapper {
 public:
  GetStatus(const system_inspector::SystemInspector* si)
      : system_inspector_(si) {}
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

  const std::string& GetBaseRoute() override {
    return kBaseRoute;
  }

 private:
  static const std::string kBaseRoute;

  const system_inspector::SystemInspector* system_inspector_;
};

} /* namespace collector */

#endif /* _GET_STATUS_H_ */
