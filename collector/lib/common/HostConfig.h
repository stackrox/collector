#ifndef COLLECTOR_HOSTCONFIG_H
#define COLLECTOR_HOSTCONFIG_H

#include <optional>
#include <string>

#include "CollectionMethod.h"

//
// The HostConfig is runtime-configured based on the host where
// collector is running. It is intended to inform minor adjustments to
// the CollectorConfig, in cases where we can fall back to other ways
// of operating.
//
class HostConfig {
 public:
  HostConfig() = default;

  collector::CollectionMethod GetCollectionMethod() const { return *collection_method_; }
  bool HasCollectionMethod() const { return collection_method_.has_value(); }
  void SetCollectionMethod(collector::CollectionMethod method) {
    collection_method_ = method;
  }

  unsigned int GetNumPossibleCPUs() const { return num_possible_cpus_; }
  void SetNumPossibleCPUs(unsigned int value) { num_possible_cpus_ = value; }

 private:
  std::optional<collector::CollectionMethod> collection_method_;
  unsigned int num_possible_cpus_;
};

#endif  // COLLECTOR_HOSTCONFIG_H
