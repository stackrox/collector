#ifndef _GET_STATUS_H_
#define _GET_STATUS_H_

#include <string>

#include "CivetServer.h"
#include "system-inspector/SystemInspector.h"

namespace collector {

class GetStatus : public CivetHandler {
 public:
  GetStatus(std::string node_name, const system_inspector::SystemInspector* si)
      : node_name_(std::move(node_name)), system_inspector_(si) {}
  bool handleGet(CivetServer* server, struct mg_connection* conn);

 private:
  std::string node_name_;
  const system_inspector::SystemInspector* system_inspector_;
};

} /* namespace collector */

#endif /* _GET_STATUS_H_ */
