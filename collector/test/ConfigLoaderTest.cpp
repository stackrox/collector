#include <gtest/gtest.h>

#include "ConfigLoader.h"

namespace collector {
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
    ASSERT_TRUE(ConfigLoader::LoadConfiguration(config, yamlNode));

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
    ASSERT_TRUE(ConfigLoader::LoadConfiguration(config, yamlNode));

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_FALSE(runtime_config.has_value());
  }
}

TEST(CollectorConfigTest, TestYamlConfigToConfigEmpty) {
  YAML::Node yamlNode = YAML::Load("");
  CollectorConfig config;
  ASSERT_FALSE(ConfigLoader::LoadConfiguration(config, yamlNode));

  auto runtime_config = config.GetRuntimeConfig();

  EXPECT_FALSE(runtime_config.has_value());
}
}  // namespace collector
