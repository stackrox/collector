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

 private:
  std::optional<collector::CollectionMethod> collection_method_;
};

#endif  // COLLECTOR_HOSTCONFIG_H
