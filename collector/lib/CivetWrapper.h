#ifndef CIVET_WRAPPER_H
#define CIVET_WRAPPER_H

#include <CivetServer.h>

namespace collector {
class CivetWrapper : public CivetHandler {
 public:
  virtual const std::string& GetBaseRoute() = 0;
};
}  // namespace collector

#endif
