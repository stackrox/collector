#ifndef _GET_STATUS_H_
#define _GET_STATUS_H_

#include <string>

#include "Sysdig.h"
#include "civetweb/CivetServer.h"

namespace collector {

class GetStatus : public CivetHandler {
 public:
  GetStatus(std::string node_name, const Sysdig* sysdig)
      : node_name_(std::move(node_name)), sysdig_(sysdig) {}
  bool handleGet(CivetServer* server, struct mg_connection* conn);

 private:
  std::string node_name_;
  const Sysdig* sysdig_;
};

} /* namespace collector */

#endif /* _GET_STATUS_H_ */
