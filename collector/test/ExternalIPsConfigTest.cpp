#include <sstream>

#include <internalapi/sensor/collector.pb.h>

#include "ExternalIPsConfig.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

namespace {

using Direction = ExternalIPsConfig::Direction;

// Test default constructor with NONE direction
TEST(ExternalIPsConfigTest, DefaultConstructorNone) {
  ExternalIPsConfig config(Direction::NONE);

  EXPECT_EQ(Direction::NONE, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test default constructor with INGRESS direction
TEST(ExternalIPsConfigTest, DefaultConstructorIngress) {
  ExternalIPsConfig config(Direction::INGRESS);

  EXPECT_EQ(Direction::INGRESS, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test default constructor with EGRESS direction
TEST(ExternalIPsConfigTest, DefaultConstructorEgress) {
  ExternalIPsConfig config(Direction::EGRESS);

  EXPECT_EQ(Direction::EGRESS, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test default constructor with BOTH direction
TEST(ExternalIPsConfigTest, DefaultConstructorBoth) {
  ExternalIPsConfig config(Direction::BOTH);

  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config constructor with no runtime config and default_enabled=false
TEST(ExternalIPsConfigTest, NoRuntimeConfigDefaultDisabled) {
  ExternalIPsConfig config(std::nullopt, false);

  EXPECT_EQ(Direction::NONE, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config constructor with no runtime config and default_enabled=true
TEST(ExternalIPsConfigTest, NoRuntimeConfigDefaultEnabled) {
  ExternalIPsConfig config(std::nullopt, true);

  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config with external IPs explicitly disabled
TEST(ExternalIPsConfigTest, RuntimeConfigDisabled) {
  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();
  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::DISABLED);

  ExternalIPsConfig config(runtime_config, true);

  EXPECT_EQ(Direction::NONE, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config with external IPs enabled and direction set to INGRESS
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

// Test runtime config with external IPs enabled and direction set to EGRESS
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

// Test runtime config with external IPs enabled and direction set to BOTH
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledBoth) {
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

// Test runtime config with external IPs enabled but direction unspecified (default case)
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledDefaultDirection) {
  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();
  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  // Don't set direction, let it use default

  ExternalIPsConfig config(runtime_config, false);

  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test that default_enabled is ignored when runtime_config is provided
TEST(ExternalIPsConfigTest, RuntimeConfigOverridesDefault) {
  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();
  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::DISABLED);

  // Even with default_enabled=true, runtime config should disable it
  ExternalIPsConfig config(runtime_config, true);

  EXPECT_EQ(Direction::NONE, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test output stream operator for NONE direction
TEST(ExternalIPsConfigTest, OutputStreamNone) {
  ExternalIPsConfig config(Direction::NONE);
  std::ostringstream oss;
  oss << config;

  EXPECT_EQ("direction(NONE)", oss.str());
}

// Test output stream operator for INGRESS direction
TEST(ExternalIPsConfigTest, OutputStreamIngress) {
  ExternalIPsConfig config(Direction::INGRESS);
  std::ostringstream oss;
  oss << config;

  EXPECT_EQ("direction(INGRESS)", oss.str());
}

// Test output stream operator for EGRESS direction
TEST(ExternalIPsConfigTest, OutputStreamEgress) {
  ExternalIPsConfig config(Direction::EGRESS);
  std::ostringstream oss;
  oss << config;

  EXPECT_EQ("direction(EGRESS)", oss.str());
}

// Test output stream operator for BOTH direction
TEST(ExternalIPsConfigTest, OutputStreamBoth) {
  ExternalIPsConfig config(Direction::BOTH);
  std::ostringstream oss;
  oss << config;

  EXPECT_EQ("direction(BOTH)", oss.str());
}

// Test bitwise operations - BOTH should match INGRESS | EGRESS
TEST(ExternalIPsConfigTest, BitwiseDirectionValues) {
  EXPECT_EQ(Direction::BOTH, Direction::INGRESS | Direction::EGRESS);
  EXPECT_EQ(static_cast<int>(Direction::NONE), 0);
  EXPECT_EQ(static_cast<int>(Direction::INGRESS), 1 << 0);
  EXPECT_EQ(static_cast<int>(Direction::EGRESS), 1 << 1);
}

}  // namespace

}  // namespace collector
