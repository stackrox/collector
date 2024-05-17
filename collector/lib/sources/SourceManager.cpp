
#include "SourceManager.h"

namespace collector {
namespace sources {

std::shared_ptr<Signal> SourceManager::Next() {
  return std::shared_ptr<Signal>();
}

}  // namespace sources
}  // namespace collector
