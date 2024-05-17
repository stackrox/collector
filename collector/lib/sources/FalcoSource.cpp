#include "FalcoSource.h"

namespace collector {
namespace sources {

std::shared_ptr<Signal> FalcoSource::Next() {
  return std::shared_ptr<Signal>();
}

}  // namespace sources
}  // namespace collector
