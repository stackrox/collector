#ifndef COLLECTOR_HOSTCONFIG_H
#define COLLECTOR_HOSTCONFIG_H

#include <string>

//
// The HostConfig is runtime-configured based on the host we are
// running collector. It is intended to inform minor adjustments to
// the CollectorConfig, in cases where we can fall back to other ways
// of operating (particularly around the collection method)
//
class HostConfig {
 public:
  HostConfig() = default;

  const std::string& CollectionMethod() const { return collection_method_; }
  bool HasCollectionMethod() const { return !collection_method_.empty(); }
  void SetCollectionMethod(std::string method) { collection_method_ = std::move(method); }

 private:
  std::string collection_method_;
};

#endif  // COLLECTOR_HOSTCONFIG_H
