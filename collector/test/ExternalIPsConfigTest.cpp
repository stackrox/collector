#include <optional>
#include <sstream>

#include <internalapi/sensor/collector.pb.h>

#include "ExternalIPsConfig.h"
#include "gtest/gtest.h"

using Direction = collector::ExternalIPsConfig::Direction;

namespace collector {

// Test constructor with no runtime config and default_enabled = false
TEST(ExternalIPsConfigTest, NoRuntimeConfigDefaultDisabled) {
  ExternalIPsConfig config(std::nullopt, false);

  EXPECT_EQ(Direction::NONE, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test constructor with no runtime config and default_enabled = true
TEST(ExternalIPsConfigTest, NoRuntimeConfigDefaultEnabled) {
  ExternalIPsConfig config(std::nullopt, true);

  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test constructor with runtime config disabled
TEST(ExternalIPsConfigTest, RuntimeConfigDisabled) {
  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();
  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::DISABLED);

  // Even with default_enabled = true, runtime config should take precedence
  ExternalIPsConfig config(runtime_config, true);

  EXPECT_EQ(Direction::NONE, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config enabled with INGRESS direction
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledIngress) {
  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();
  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  external_ips_config->set_direction(sensor::ExternalIpsDirection::INGRESS);

  ExternalIPsConfig config(runtime_config, false);

  EXPECT_EQ(Direction::INGRESS, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config enabled with EGRESS direction
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledEgress) {
  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();
  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  external_ips_config->set_direction(sensor::ExternalIpsDirection::EGRESS);

  ExternalIPsConfig config(runtime_config, false);

  EXPECT_EQ(Direction::EGRESS, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config enabled with BOTH direction (explicit)
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledBothExplicit) {
  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();
  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  external_ips_config->set_direction(sensor::ExternalIpsDirection::BOTH);

  ExternalIPsConfig config(runtime_config, false);

  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config enabled with UNSPECIFIED direction (defaults to BOTH)
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledUnspecified) {
  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();
  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  external_ips_config->set_direction(sensor::ExternalIpsDirection::UNSPECIFIED);

  ExternalIPsConfig config(runtime_config, false);

  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test default constructor
TEST(ExternalIPsConfigTest, DefaultConstructor) {
  ExternalIPsConfig config;

  EXPECT_EQ(Direction::NONE, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test constructor with explicit direction
TEST(ExternalIPsConfigTest, ConstructorWithDirection) {
  ExternalIPsConfig config_ingress(Direction::INGRESS);
  EXPECT_EQ(Direction::INGRESS, config_ingress.GetDirection());
  EXPECT_TRUE(config_ingress.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config_ingress.IsEnabled(Direction::EGRESS));

  ExternalIPsConfig config_egress(Direction::EGRESS);
  EXPECT_EQ(Direction::EGRESS, config_egress.GetDirection());
  EXPECT_FALSE(config_egress.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config_egress.IsEnabled(Direction::EGRESS));

  ExternalIPsConfig config_both(Direction::BOTH);
  EXPECT_EQ(Direction::BOTH, config_both.GetDirection());
  EXPECT_TRUE(config_both.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config_both.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config_both.IsEnabled(Direction::BOTH));
}

// Test ostream operator for NONE
TEST(ExternalIPsConfigTest, OstreamOperatorNone) {
  ExternalIPsConfig config(Direction::NONE);
  std::ostringstream os;
  os << config;
  EXPECT_EQ("direction(NONE)", os.str());
}

// Test ostream operator for INGRESS
TEST(ExternalIPsConfigTest, OstreamOperatorIngress) {
  ExternalIPsConfig config(Direction::INGRESS);
  std::ostringstream os;
  os << config;
  EXPECT_EQ("direction(INGRESS)", os.str());
}

// Test ostream operator for EGRESS
TEST(ExternalIPsConfigTest, OstreamOperatorEgress) {
  ExternalIPsConfig config(Direction::EGRESS);
  std::ostringstream os;
  os << config;
  EXPECT_EQ("direction(EGRESS)", os.str());
}

// Test ostream operator for BOTH
TEST(ExternalIPsConfigTest, OstreamOperatorBoth) {
  ExternalIPsConfig config(Direction::BOTH);
  std::ostringstream os;
  os << config;
  EXPECT_EQ("direction(BOTH)", os.str());
}

}  // namespace collector
