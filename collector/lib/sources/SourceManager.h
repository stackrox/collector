#include <memory>
#include <vector>

#include "Source.h"

namespace collector {
namespace sources {

class SourceManager {
 public:
  SourceManager();

  std::shared_ptr<Signal> Next();

 private:
  // maybe a priority queue?
  std::vector<std::unique_ptr<ISource>> sources_;
};

}  // namespace sources
}  // namespace collector
