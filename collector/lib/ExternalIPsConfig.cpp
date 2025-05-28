#include "ExternalIPsConfig.h"

namespace collector {

ExternalIPsConfig::ExternalIPsConfig(std::optional<sensor::CollectorConfig> runtime_config, bool default_enabled) {
  if (!runtime_config.has_value()) {
    direction_enabled_ = default_enabled ? Direction::BOTH : Direction::NONE;
    return;
  }

  // At this point we know runtime_config has a value, we can access it directly
  const auto& external_ips = runtime_config->networking().external_ips();
  if (external_ips.enabled() != sensor::ExternalIpsEnabled::ENABLED) {
    direction_enabled_ = Direction::NONE;
    return;
  }

  switch (external_ips.direction()) {
    case sensor::ExternalIpsDirection::INGRESS:
      direction_enabled_ = Direction::INGRESS;
      break;
    case sensor::ExternalIpsDirection::EGRESS:
      direction_enabled_ = Direction::EGRESS;
      break;
    default:
      direction_enabled_ = Direction::BOTH;
      break;
  }
}

std::ostream& operator<<(std::ostream& os, const ExternalIPsConfig& config) {
  os << "direction(";

  switch (config.GetDirection()) {
    case ExternalIPsConfig::Direction::NONE:
      os << "NONE";
      break;
    case ExternalIPsConfig::Direction::INGRESS:
      os << "INGRESS";
      break;
    case ExternalIPsConfig::Direction::EGRESS:
      os << "EGRESS";
      break;
    case ExternalIPsConfig::Direction::BOTH:
      os << "BOTH";
      break;
    default:
      os << "invalid";
      break;
  }

  return os << ")";
}

}  // namespace collector