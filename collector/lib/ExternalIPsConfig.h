#pragma once

#include <assert.h>
#include <optional>
#include <ostream>

#include <internalapi/sensor/collector.pb.h>

namespace collector {

/// Controls whether external (non-cluster) IPs are preserved or aggregated
/// to a canonical sentinel address. Configurable per direction (ingress vs
/// egress) because egress destinations are typically more security-relevant,
/// while ingress sources are often load-balancer/CDN noise. Direction is a
/// bitmask so BOTH can be tested with a single bitwise AND.
class ExternalIPsConfig {
 public:
  enum Direction {
    NONE = 0,
    INGRESS = 1 << 0,
    EGRESS = 1 << 1,
    BOTH = INGRESS | EGRESS,
  };

  // Are External-IPs enabled in the provided direction ?
  bool IsEnabled(Direction direction) const {
    assert(direction != Direction::NONE);
    return (direction & direction_enabled_) == direction;
  }

  // Direction in which External-IPs are enabled
  Direction GetDirection() const { return direction_enabled_; }

  // Extract the External-IPs configuration from the provided runtime-conf.
  // If the runtime-configuration is unset then 'default_enabled' is used
  // as a fallback to enable in both directions.
  // 'runtime_config' should be locked prior to calling.
  ExternalIPsConfig(std::optional<sensor::CollectorConfig> runtime_config, bool default_enabled);

  ExternalIPsConfig(Direction direction = Direction::NONE) : direction_enabled_(direction) {}

 private:
  Direction direction_enabled_;
};

std::ostream& operator<<(std::ostream& os, const ExternalIPsConfig& config);

}  // end namespace collector
