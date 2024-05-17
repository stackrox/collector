#include <memory>

#include "api/v1/signal.pb.h"

namespace collector {
namespace sources {

using Signal = v1::Signal;

class ISource {
 public:
  ISource() = delete;

  virtual std::shared_ptr<Signal> Next() = 0;
};

}  // namespace sources
}  // namespace collector
