#include "Source.h"

namespace collector {
namespace sources {

class FalcoSource : public ISource {
 public:
  std::shared_ptr<Signal> Next();
};

}  // namespace sources
}  // namespace collector
