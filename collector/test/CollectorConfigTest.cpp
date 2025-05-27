#include <optional>

#include <internalapi/sensor/collector.pb.h>

#include "CollectorArgs.h"
#include "CollectorConfig.h"
#include "ConfigLoader.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;
using Direction = collector::ExternalIPsConfig::Direction;

namespace collector {

class MockCollectorConfig : public CollectorConfig {
 public:
  MockCollectorConfig() = default;

  void MockSetSinspBufferSize(unsigned int value) {
    SetSinspBufferSize(value);
  }

  void MockSetSinspTotalBufferSize(unsigned int value) {
    SetSinspTotalBufferSize(value);
  }

  void MockSetHostConfig(HostConfig* config) {
    SetHostConfig(config);
  }

  void MockSetSinspCpuPerBuffer(unsigned int value) {
    SetSinspCpuPerBuffer(value);
  }

  void MockSetEnableExternalIPs(bool value) {
    SetEnableExternalIPs(value);
  }
};

// Test that unmodified value is returned, when some dependency values are
// missing
TEST(CollectorConfigTest, TestSinspBufferSizeReturnUnmodified) {
  using namespace collector;

  MockCollectorConfig config;
  HostConfig hconfig;
  hconfig.SetNumPossibleCPUs(0);
  config.MockSetSinspCpuPerBuffer(0);
  config.MockSetSinspTotalBufferSize(0);
  config.MockSetSinspBufferSize(1);
  config.MockSetHostConfig(&hconfig);

  // CPU-per-buffer is not initialized
  EXPECT_EQ(1, config.GetSinspBufferSize());

  // Number of CPUs is not initialized
  config.MockSetSinspCpuPerBuffer(1);
  EXPECT_EQ(1, config.GetSinspBufferSize());

  // Total buffers size is not initialized
  hconfig.SetNumPossibleCPUs(1);
  config.MockSetHostConfig(&hconfig);
  EXPECT_EQ(1, config.GetSinspBufferSize());
}

// Test adjusted value
TEST(CollectorConfigTest, TestSinspCpuPerBufferAdjusted) {
  using namespace collector;

  MockCollectorConfig config;
  HostConfig hconfig;
  config.MockSetSinspTotalBufferSize(512 * 1024 * 1024);
  config.MockSetSinspBufferSize(8 * 1024 * 1024);
  config.MockSetSinspCpuPerBuffer(1);

  // Low number of CPUs, raw value
  hconfig.SetNumPossibleCPUs(16);
  config.MockSetHostConfig(&hconfig);
  EXPECT_EQ(8 * 1024 * 1024, config.GetSinspBufferSize());

  // High number of CPUs, adjusted value to power of 2
  hconfig.SetNumPossibleCPUs(150);
  config.MockSetHostConfig(&hconfig);
  EXPECT_EQ(2 * 1024 * 1024, config.GetSinspBufferSize());

  // Extreme number of CPUs, adjusted value to power of 2
  hconfig.SetNumPossibleCPUs(1024);
  config.MockSetHostConfig(&hconfig);
  EXPECT_EQ(512 * 1024, config.GetSinspBufferSize());

  // Extreme number of CPUs and low total buffer size, adjusted value is not
  // less than one page
  config.MockSetSinspTotalBufferSize(512 * 1024);
  hconfig.SetNumPossibleCPUs(1024);
  config.MockSetHostConfig(&hconfig);
  EXPECT_EQ(16384, config.GetSinspBufferSize());
}

TEST(CollectorConfigTest, TestSetRuntimeConfig) {
  MockCollectorConfig config;

  EXPECT_EQ(std::nullopt, config.GetRuntimeConfig());

  sensor::CollectorConfig runtime_config;

  config.SetRuntimeConfig(runtime_config);

  EXPECT_NE(std::nullopt, config.GetRuntimeConfig());
}

TEST(CollectorConfigTest, TestEnableExternalIpsFeatureFlag) {
  MockCollectorConfig config;

  // without the presence of the runtime configuration
  // the enable_external_ips_ flag should be used

  config.MockSetEnableExternalIPs(false);

  EXPECT_EQ(Direction::NONE, config.ExternalIPsConf().GetDirection());

  config.MockSetEnableExternalIPs(true);

  EXPECT_EQ(Direction::BOTH, config.ExternalIPsConf().GetDirection());
}

TEST(CollectorConfigTest, TestEnableExternalIpsRuntimeConfig) {
  MockCollectorConfig config;

  // With the presence of runtime config, the feature
  // flag should be ignored

  config.MockSetEnableExternalIPs(true);

  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();

  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::DISABLED);

  config.SetRuntimeConfig(runtime_config);

  EXPECT_EQ(Direction::NONE, config.ExternalIPsConf().GetDirection());
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::BOTH));

  config.MockSetEnableExternalIPs(false);

  external_ips_config->set_enabled(sensor::ExternalIpsEnabled::ENABLED);
  config.SetRuntimeConfig(runtime_config);

  EXPECT_EQ(Direction::BOTH, config.ExternalIPsConf().GetDirection());
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::BOTH));
}

TEST(CollectorConfigTest, TestIsEnabledIngress) {
  std::string yamlStr = R"(
                networking:
                  externalIps:
                    enabled: enabled
                    direction: ingress
             )";

  YAML::Node yamlNode = YAML::Load(yamlStr);
  CollectorConfig config;
  ASSERT_EQ(ConfigLoader(config).LoadConfiguration(yamlNode), ConfigLoader::SUCCESS) << "Input: " << yamlStr;

  auto runtime_config = config.GetRuntimeConfig();

  EXPECT_TRUE(runtime_config.has_value());

  EXPECT_EQ(Direction::INGRESS, config.ExternalIPsConf().GetDirection());
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::BOTH));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::NONE));
}

TEST(CollectorConfigTest, TestIsEnabledEgress) {
  std::string yamlStr = R"(
                networking:
                  externalIps:
                    enabled: enabled
                    direction: egress
             )";

  YAML::Node yamlNode = YAML::Load(yamlStr);
  CollectorConfig config;
  ASSERT_EQ(ConfigLoader(config).LoadConfiguration(yamlNode), ConfigLoader::SUCCESS) << "Input: " << yamlStr;

  auto runtime_config = config.GetRuntimeConfig();

  EXPECT_TRUE(runtime_config.has_value());

  EXPECT_EQ(Direction::EGRESS, config.ExternalIPsConf().GetDirection());
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::BOTH));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::NONE));
}

TEST(CollectorConfigTest, TestIsEnabledBoth) {
  std::string yamlStr = R"(
                networking:
                  externalIps:
                    enabled: enabled
                    direction: both
             )";

  YAML::Node yamlNode = YAML::Load(yamlStr);
  CollectorConfig config;
  ASSERT_EQ(ConfigLoader(config).LoadConfiguration(yamlNode), ConfigLoader::SUCCESS) << "Input: " << yamlStr;

  auto runtime_config = config.GetRuntimeConfig();

  EXPECT_TRUE(runtime_config.has_value());

  EXPECT_EQ(Direction::BOTH, config.ExternalIPsConf().GetDirection());
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::INGRESS));
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::EGRESS));
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::BOTH));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::NONE));
}

TEST(CollectorConfigTest, TestIsEnabledNone) {
  std::string yamlStr = R"(
                networking:
                  externalIps:
                    enabled: disabled
             )";

  YAML::Node yamlNode = YAML::Load(yamlStr);
  CollectorConfig config;
  ASSERT_EQ(ConfigLoader(config).LoadConfiguration(yamlNode), ConfigLoader::SUCCESS) << "Input: " << yamlStr;

  auto runtime_config = config.GetRuntimeConfig();

  EXPECT_TRUE(runtime_config.has_value());

  EXPECT_EQ(Direction::NONE, config.ExternalIPsConf().GetDirection());
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::INGRESS));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::EGRESS));
  EXPECT_FALSE(config.ExternalIPsConf().IsEnabled(Direction::BOTH));
  EXPECT_TRUE(config.ExternalIPsConf().IsEnabled(Direction::NONE));
}

}  // namespace collector
