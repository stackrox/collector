#include <optional>

#include <internalapi/sensor/collector.pb.h>

#include "CollectorArgs.h"
#include "CollectorConfig.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

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

  bool MockYamlConfigToConfig(YAML::Node& yamlConfig) {
    return YamlConfigToConfig(yamlConfig);
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

  EXPECT_FALSE(config.EnableExternalIPs());

  config.MockSetEnableExternalIPs(true);

  EXPECT_TRUE(config.EnableExternalIPs());
}

TEST(CollectorConfigTest, TestEnableExternalIpsRuntimeConfig) {
  MockCollectorConfig config;

  // With the presence of runtime config, the feature
  // flag should be ignored

  config.MockSetEnableExternalIPs(true);

  sensor::CollectorConfig runtime_config;
  sensor::NetworkConnectionConfig* network_config = runtime_config.mutable_network_connection_config();

  network_config->set_enable_external_ips(false);

  config.SetRuntimeConfig(runtime_config);

  EXPECT_FALSE(config.EnableExternalIPs());

  config.MockSetEnableExternalIPs(false);

  network_config->set_enable_external_ips(true);
  config.SetRuntimeConfig(runtime_config);

  EXPECT_TRUE(config.EnableExternalIPs());
}

TEST(CollectorConfigTest, TestYamlConfigToConfigMultiple) {
  std::vector<std::pair<std::string, bool>> tests = {
      {R"(
                  networkConnectionConfig:
                    enableExternalIps: true
               )",
       true},
      {R"(
                  networkConnectionConfig:
                    enableExternalIps: false
               )",
       false},
      {R"(
                  networkConnectionConfig:
               )",
       false},
      {R"(
                  networkConnectionConfig:
                    unknownField: asdf
               )",
       false}};

  for (const auto& [yamlStr, expected] : tests) {
    YAML::Node yamlNode = YAML::Load(yamlStr);

    MockCollectorConfig config;

    bool result = config.MockYamlConfigToConfig(yamlNode);
    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_TRUE(result);
    EXPECT_TRUE(runtime_config.has_value());

    const auto& cfg = runtime_config.value();
    const auto& network_cfg = cfg.network_connection_config();
    EXPECT_EQ(network_cfg.enable_external_ips(), expected);
    EXPECT_EQ(config.EnableExternalIPs(), expected);
  }
}

TEST(CollectorConfigTest, TestYamlConfigToConfigInvalid) {
  std::string yamlStr = R"(
                  unknownField: asdf
               )";

  YAML::Node yamlNode = YAML::Load(yamlStr);

  MockCollectorConfig config;

  bool result = config.MockYamlConfigToConfig(yamlNode);
  auto runtime_config = config.GetRuntimeConfig();

  EXPECT_FALSE(result);
  EXPECT_FALSE(runtime_config.has_value());
}

TEST(CollectorConfigTest, TestYamlConfigToConfigEmpty) {
  std::string yamlStr = R"()";
  YAML::Node yamlNode = YAML::Load(yamlStr);

  MockCollectorConfig config;

  EXPECT_DEATH({ config.MockYamlConfigToConfig(yamlNode); }, ".*");
}

}  // namespace collector
