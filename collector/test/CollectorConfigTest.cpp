#include <optional>

#include <internalapi/sensor/collector.pb.h>

#include "CollectorConfig.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

namespace collector {

// Test that unmodified value is returned, when some dependency values are
// missing
TEST(CollectorConfigTest, TestSinspBufferSizeReturnUnmodified) {
  using namespace collector;

  CollectorConfig config;
  HostConfig hconfig;
  hconfig.SetNumPossibleCPUs(0);
  config.SetSinspCpuPerBuffer(0);
  config.SetSinspTotalBufferSize(0);
  config.SetSinspBufferSize(1);
  config.SetHostConfig(&hconfig);

  // CPU-per-buffer is not initialized
  EXPECT_EQ(1, config.GetSinspBufferSize());

  // Number of CPUs is not initialized
  config.SetSinspCpuPerBuffer(1);
  EXPECT_EQ(1, config.GetSinspBufferSize());

  // Total buffers size is not initialized
  hconfig.SetNumPossibleCPUs(1);
  config.SetHostConfig(&hconfig);
  EXPECT_EQ(1, config.GetSinspBufferSize());
}

// Test adjusted value
TEST(CollectorConfigTest, TestSinspCpuPerBufferAdjusted) {
  using namespace collector;

  CollectorConfig config;
  HostConfig hconfig;
  config.SetSinspTotalBufferSize(512 * 1024 * 1024);
  config.SetSinspBufferSize(8 * 1024 * 1024);
  config.SetSinspCpuPerBuffer(1);

  // Low number of CPUs, raw value
  hconfig.SetNumPossibleCPUs(16);
  config.SetHostConfig(&hconfig);
  EXPECT_EQ(8 * 1024 * 1024, config.GetSinspBufferSize());

  // High number of CPUs, adjusted value to power of 2
  hconfig.SetNumPossibleCPUs(150);
  config.SetHostConfig(&hconfig);
  EXPECT_EQ(2 * 1024 * 1024, config.GetSinspBufferSize());

  // Extreme number of CPUs, adjusted value to power of 2
  hconfig.SetNumPossibleCPUs(1024);
  config.SetHostConfig(&hconfig);
  EXPECT_EQ(512 * 1024, config.GetSinspBufferSize());

  // Extreme number of CPUs and low total buffer size, adjusted value is not
  // less than one page
  config.SetSinspTotalBufferSize(512 * 1024);
  hconfig.SetNumPossibleCPUs(1024);
  config.SetHostConfig(&hconfig);
  EXPECT_EQ(16384, config.GetSinspBufferSize());
}

TEST(CollectorConfigTest, TestSetRuntimeConfig) {
  CollectorConfig config;

  EXPECT_EQ(std::nullopt, config.GetRuntimeConfig());

  sensor::CollectorConfig runtime_config;

  config.SetRuntimeConfig(runtime_config);

  EXPECT_NE(std::nullopt, config.GetRuntimeConfig());
}

TEST(CollectorConfigTest, TestEnableExternalIpsFeatureFlag) {
  CollectorConfig config;

  // without the presence of the runtime configuration
  // the enable_external_ips_ flag should be used

  config.SetEnableExternalIPs(false);

  EXPECT_FALSE(config.EnableExternalIPs());

  config.SetEnableExternalIPs(true);

  EXPECT_TRUE(config.EnableExternalIPs());
}

TEST(CollectorConfigTest, TestEnableExternalIpsRuntimeConfig) {
  CollectorConfig config;

  // With the presence of runtime config, the feature
  // flag should be ignored

  config.SetEnableExternalIPs(true);

  sensor::CollectorConfig runtime_config;
  auto* networking_config = runtime_config.mutable_networking();
  auto* external_ips_config = networking_config->mutable_external_ips();

  external_ips_config->set_enable(false);

  config.SetRuntimeConfig(runtime_config);

  EXPECT_FALSE(config.EnableExternalIPs());

  config.SetEnableExternalIPs(false);

  external_ips_config->set_enable(true);
  config.SetRuntimeConfig(runtime_config);

  EXPECT_TRUE(config.EnableExternalIPs());
}

TEST(CollectorConfigTest, TestYamlConfigToConfigMultiple) {
  std::vector<std::pair<std::string, bool>> tests = {
      {R"(
                  networking:
                    externalIps:
                      enable: true
               )",
       true},
      {R"(
                  networking:
                    externalIps:
                      enable: false
               )",
       false},
      {R"(
                  networking:
                    externalIps:
               )",
       false},
  };

  for (const auto& [yamlStr, expected] : tests) {
    YAML::Node yamlNode = YAML::Load(yamlStr);

    CollectorConfig config;

    config.YamlConfigToConfig(yamlNode);
    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_TRUE(runtime_config.has_value());

    bool enabled = runtime_config.value()
                       .networking()
                       .external_ips()
                       .enable();
    EXPECT_EQ(enabled, expected);
    EXPECT_EQ(config.EnableExternalIPs(), expected);
  }
}

TEST(CollectorConfigTest, TestYamlConfigToConfigInvalid) {
  std::vector<std::string> tests = {
      R"(
                  networking:
               )",
      R"(
                  networking:
                    unknownFiled: asdf
               )",
      R"(
                  unknownField: asdf
               )"};

  for (const auto& yamlStr : tests) {
    YAML::Node yamlNode = YAML::Load(yamlStr);

    CollectorConfig config;

    config.YamlConfigToConfig(yamlNode);
    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_FALSE(runtime_config.has_value());
  }
}

TEST(CollectorConfigTest, TestYamlConfigToConfigEmpty) {
  std::string yamlStr = R"()";
  YAML::Node yamlNode = YAML::Load(yamlStr);

  CollectorConfig config;

  EXPECT_DEATH({ config.YamlConfigToConfig(yamlNode); }, ".*");
}

}  // namespace collector
