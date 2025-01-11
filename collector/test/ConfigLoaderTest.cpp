#include <gtest/gtest.h>

#include "ConfigLoader.h"

namespace collector {
TEST(CollectorConfigTest, TestYamlConfigToConfigMultiple) {
  std::vector<std::pair<std::string, bool>> tests = {
      {R"(
                  networking:
                    externalIps:
                      enabled: ENABLED
               )",
       true},
      {R"(
                  networking:
                    externalIps:
                      enabled: DISABLED
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
    ASSERT_TRUE(ConfigLoader::LoadConfiguration(config, yamlNode));

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_TRUE(runtime_config.has_value());

    bool enabled = runtime_config.value()
                       .networking()
                       .external_ips()
                       .enabled() == sensor::ExternalIpsEnabled::ENABLED;
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
    ASSERT_TRUE(ConfigLoader::LoadConfiguration(config, yamlNode));

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_FALSE(runtime_config.has_value());
  }
}

TEST(CollectorConfigTest, TestYamlConfigToConfigEmptyOrMalformed) {
  std::vector<std::string> tests = {
      R"(
                  asdf
               )",
      R"()"};

  for (const auto& yamlStr : tests) {
    YAML::Node yamlNode = YAML::Load(yamlStr);
    CollectorConfig config;
    ASSERT_FALSE(ConfigLoader::LoadConfiguration(config, yamlNode));

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_FALSE(runtime_config.has_value());
  }
}

TEST(CollectorConfigTest, TestMaxConnectionsPerMinute) {
  std::vector<std::pair<std::string, int>> tests = {
      {R"(
                  networking:
                    externalIps:
                      enabled: DISABLED
                    maxConnectionsPerMinute: 1234
               )",
       1234},
      {R"(
                  networking:
                    externalIps:
                      enabled: DISABLED
                    maxConnectionsPerMinute: 1337
               )",
       1337},
      {R"(
                  networking:
                    externalIps:
                      enabled: DISABLED
                    maxConnectionsPerMinute: invalid
               )",
       1024},
  };

  for (const auto& [yamlStr, expected] : tests) {
    YAML::Node yamlNode = YAML::Load(yamlStr);
    CollectorConfig config;
    ASSERT_TRUE(ConfigLoader::LoadConfiguration(config, yamlNode));

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_TRUE(runtime_config.has_value());

    int rate = runtime_config.value()
                   .networking()
                   .max_connections_per_minute();
    EXPECT_EQ(rate, expected);
    EXPECT_EQ(config.MaxConnectionsPerMinute(), expected);
  }
}
}  // namespace collector
