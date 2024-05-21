#include <memory>

#include "api/v1/signal.pb.h"

namespace collector {
namespace sources {

using Signal = v1::Signal;

class ISource {
 public:
  virtual std::shared_ptr<Signal> Next() = 0;
  virtual bool Init(const CollectorConfig& config) override;
  virtual void Start();
  virtual void Stop();
};

}  // namespace sources
}  // namespace collector
