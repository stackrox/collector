#include <sstream>

#include <internalapi/sensor/collector.pb.h>

#include "ExternalIPsConfig.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using Direction = collector::ExternalIPsConfig::Direction;

namespace collector {

// Test default constructor
TEST(ExternalIPsConfigTest, DefaultConstructor) {
  ExternalIPsConfig config;
  EXPECT_EQ(Direction::NONE, config.GetDirection());
}

TEST(ExternalIPsConfigTest, DefaultConstructorWithDirection) {
  ExternalIPsConfig config_none(Direction::NONE);
  EXPECT_EQ(Direction::NONE, config_none.GetDirection());

  ExternalIPsConfig config_ingress(Direction::INGRESS);
  EXPECT_EQ(Direction::INGRESS, config_ingress.GetDirection());

  ExternalIPsConfig config_egress(Direction::EGRESS);
  EXPECT_EQ(Direction::EGRESS, config_egress.GetDirection());

  ExternalIPsConfig config_both(Direction::BOTH);
  EXPECT_EQ(Direction::BOTH, config_both.GetDirection());
}

// Test constructor with no runtime config, default_enabled = false
TEST(ExternalIPsConfigTest, NoRuntimeConfigDefaultDisabled) {
  ExternalIPsConfig config(std::nullopt, false);
  EXPECT_EQ(Direction::NONE, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test constructor with no runtime config, default_enabled = true
TEST(ExternalIPsConfigTest, NoRuntimeConfigDefaultEnabled) {
  ExternalIPsConfig config(std::nullopt, true);
  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config with external IPs disabled
TEST(ExternalIPsConfigTest, RuntimeConfigDisabled) {
  sensor::CollectorConfig runtime_config;
  auto* networking = runtime_config.mutable_networking();
  auto* external_ips = networking->mutable_external_ips();
  external_ips->set_enabled(sensor::ExternalIpsEnabled::DISABLED);

  // default_enabled should be ignored when runtime_config is present
  ExternalIPsConfig config_true(runtime_config, true);
  EXPECT_EQ(Direction::NONE, config_true.GetDirection());
  EXPECT_FALSE(config_true.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config_true.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config_true.IsEnabled(Direction::BOTH));

  ExternalIPsConfig config_false(runtime_config, false);
  EXPECT_EQ(Direction::NONE, config_false.GetDirection());
  EXPECT_FALSE(config_false.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config_false.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config_false.IsEnabled(Direction::BOTH));
}

// Test runtime config enabled with INGRESS direction
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledIngress) {
  sensor::CollectorConfig runtime_config;
  auto* networking = runtime_config.mutable_networking();
  auto* external_ips = networking->mutable_external_ips();
  external_ips->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  external_ips->set_direction(sensor::ExternalIpsDirection::INGRESS);

  ExternalIPsConfig config(runtime_config, false);
  EXPECT_EQ(Direction::INGRESS, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config enabled with EGRESS direction
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledEgress) {
  sensor::CollectorConfig runtime_config;
  auto* networking = runtime_config.mutable_networking();
  auto* external_ips = networking->mutable_external_ips();
  external_ips->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  external_ips->set_direction(sensor::ExternalIpsDirection::EGRESS);

  ExternalIPsConfig config(runtime_config, false);
  EXPECT_EQ(Direction::EGRESS, config.GetDirection());
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config enabled with BOTH direction
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledBoth) {
  sensor::CollectorConfig runtime_config;
  auto* networking = runtime_config.mutable_networking();
  auto* external_ips = networking->mutable_external_ips();
  external_ips->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  external_ips->set_direction(sensor::ExternalIpsDirection::BOTH);

  ExternalIPsConfig config(runtime_config, false);
  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test runtime config enabled with UNSPECIFIED direction (defaults to BOTH)
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledUnspecified) {
  sensor::CollectorConfig runtime_config;
  auto* networking = runtime_config.mutable_networking();
  auto* external_ips = networking->mutable_external_ips();
  external_ips->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  external_ips->set_direction(sensor::ExternalIpsDirection::UNSPECIFIED);

  ExternalIPsConfig config(runtime_config, false);
  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test Direction enum bitwise operations
TEST(ExternalIPsConfigTest, DirectionBitwiseOperations) {
  // Verify that BOTH is the bitwise OR of INGRESS and EGRESS
  EXPECT_EQ(Direction::BOTH, static_cast<Direction>(Direction::INGRESS | Direction::EGRESS));

  // Verify bitwise AND operations for checking enabled state
  EXPECT_EQ(Direction::INGRESS, static_cast<Direction>(Direction::BOTH & Direction::INGRESS));
  EXPECT_EQ(Direction::EGRESS, static_cast<Direction>(Direction::BOTH & Direction::EGRESS));
  EXPECT_EQ(Direction::NONE, static_cast<Direction>(Direction::INGRESS & Direction::EGRESS));
}

// Test IsEnabled with all Direction combinations
TEST(ExternalIPsConfigTest, IsEnabledNone) {
  ExternalIPsConfig config(Direction::NONE);
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

TEST(ExternalIPsConfigTest, IsEnabledIngressOnly) {
  ExternalIPsConfig config(Direction::INGRESS);
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

TEST(ExternalIPsConfigTest, IsEnabledEgressOnly) {
  ExternalIPsConfig config(Direction::EGRESS);
  EXPECT_FALSE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.IsEnabled(Direction::BOTH));
}

TEST(ExternalIPsConfigTest, IsEnabledBoth) {
  ExternalIPsConfig config(Direction::BOTH);
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

// Test stream operator output
TEST(ExternalIPsConfigTest, StreamOperatorNone) {
  ExternalIPsConfig config(Direction::NONE);
  std::ostringstream oss;
  oss << config;
  EXPECT_EQ("direction(NONE)", oss.str());
}

TEST(ExternalIPsConfigTest, StreamOperatorIngress) {
  ExternalIPsConfig config(Direction::INGRESS);
  std::ostringstream oss;
  oss << config;
  EXPECT_EQ("direction(INGRESS)", oss.str());
}

TEST(ExternalIPsConfigTest, StreamOperatorEgress) {
  ExternalIPsConfig config(Direction::EGRESS);
  std::ostringstream oss;
  oss << config;
  EXPECT_EQ("direction(EGRESS)", oss.str());
}

TEST(ExternalIPsConfigTest, StreamOperatorBoth) {
  ExternalIPsConfig config(Direction::BOTH);
  std::ostringstream oss;
  oss << config;
  EXPECT_EQ("direction(BOTH)", oss.str());
}

// Test runtime config enabled without explicit direction field
TEST(ExternalIPsConfigTest, RuntimeConfigEnabledNoDirection) {
  sensor::CollectorConfig runtime_config;
  auto* networking = runtime_config.mutable_networking();
  auto* external_ips = networking->mutable_external_ips();
  external_ips->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  // direction field is not set, should default to UNSPECIFIED which becomes BOTH

  ExternalIPsConfig config(runtime_config, false);
  EXPECT_EQ(Direction::BOTH, config.GetDirection());
  EXPECT_TRUE(config.IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.IsEnabled(Direction::BOTH));
}

}  // namespace collector
