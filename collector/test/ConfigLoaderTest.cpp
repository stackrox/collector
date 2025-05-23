#include <sstream>

#include <gtest/gtest.h>

#include <google/protobuf/util/message_differencer.h>

#include "internalapi/sensor/collector.pb.h"

#include "ConfigLoader.h"
#include "proto/test-config.pb.h"

namespace collector {
using namespace google::protobuf::util;
using Direction = ExternalIPsConfig::Direction;

std::string ErrorsToString(const std::vector<ParserError>& errors) {
  std::stringstream ss;
  for (size_t i = 0; i < errors.size(); i++) {
    ss << i << ": " << errors.at(i).What() << std::endl;
  }
  return ss.str();
}

constexpr std::array ValidationModeStr{
    "STRICT",
    "PERMISSIVE",
    "UNKNOWN_FIELDS_ONLY",
};

/*
 * Generic yaml parser tests
 */
TEST(TestParserYaml, Parsing) {
  struct TestCase {
    std::string input;
    test_config::Config expected;
  };

  test_config::Config all_fields;
  all_fields.set_enabled(true);
  all_fields.set_field_i32(-32);
  all_fields.set_field_u32(32);
  all_fields.set_field_i64(-64);
  all_fields.set_field_u64(64);
  all_fields.set_field_double(3.14);
  all_fields.set_field_float(0.12345);
  all_fields.set_field_string("Yes, this is some random string for testing");
  all_fields.mutable_field_message()->set_enabled(true);
  all_fields.mutable_field_repeated()->Add(1);
  all_fields.mutable_field_repeated()->Add(2);
  all_fields.mutable_field_repeated()->Add(3);
  all_fields.set_field_enum(test_config::EnumField::TYPE2);
  all_fields.mutable_field_repeated_enum()->Add(0);
  all_fields.mutable_field_repeated_enum()->Add(1);

  std::vector<TestCase> tests = {
      {R"()", {}},
      {R"(
            enabled: true
            fieldI32: -32
            fieldU32: 32
            fieldI64: -64
            fieldU64: 64
            fieldDouble: 3.14
            fieldFloat: 0.12345
            fieldString: Yes, this is some random string for testing
            fieldMessage:
                enabled: true
            fieldRepeated:
                - 1
                - 2
                - 3
            fieldEnum: type2
            fieldRepeatedEnum:
                - type1
                - TYPE2
        )",
       all_fields},
  };

  for (const auto& [input, expected] : tests) {
    test_config::Config parsed;
    ParserYaml parser("/test.yml");
    parser.Parse(&parsed, YAML::Load(input));

    bool equals = MessageDifferencer::Equals(parsed, expected);

    ASSERT_TRUE(equals) << "### parsed: " << std::endl
                        << parsed.DebugString() << std::endl
                        << "### expected: " << std::endl
                        << expected.DebugString();
  }
}

TEST(TestParserYaml, OverwrittingFields) {
  test_config::Config cfg;
  std::string input = R"(
        enabled: false
        fieldI32: -1234
        fieldU32: 4321
        fieldRepeated:
            - 15
        fieldEnum: TYPE1
        fieldRepeatedEnum:
            - TYPE1
    )";

  test_config::Config expected;
  expected.set_enabled(false);
  expected.set_field_i32(-1234);
  expected.set_field_u32(4321);
  expected.mutable_field_repeated()->Add(15);
  expected.set_field_enum(test_config::EnumField::TYPE1);
  expected.mutable_field_repeated_enum()->Add(0);
  ParserYaml parser("/test.yml");
  parser.Parse(&cfg, YAML::Load(input));

  bool equals = MessageDifferencer::Equals(cfg, expected);
  ASSERT_TRUE(equals) << "### parsed: " << std::endl
                      << cfg.DebugString() << std::endl
                      << "### expected: " << std::endl
                      << expected.DebugString();

  input = R"(
        enabled: true
        fieldU32: 1234
        fieldRepeated:
            - 1
            - 2
            - 3
        fieldEnum: TYPE2
        fieldRepeatedEnum:
            - TYPE2
            - TYPE2
    )";

  expected.set_enabled(true);
  expected.set_field_u32(1234);
  expected.mutable_field_repeated()->Clear();
  expected.mutable_field_repeated()->Add(1);
  expected.mutable_field_repeated()->Add(2);
  expected.mutable_field_repeated()->Add(3);
  expected.set_field_enum(test_config::EnumField::TYPE2);
  expected.mutable_field_repeated_enum()->Clear();
  expected.mutable_field_repeated_enum()->Add(1);
  expected.mutable_field_repeated_enum()->Add(1);

  parser.Parse(&cfg, YAML::Load(input));

  equals = MessageDifferencer::Equals(cfg, expected);
  ASSERT_TRUE(equals) << "### parsed: " << std::endl
                      << cfg.DebugString() << std::endl
                      << "### expected: " << std::endl
                      << expected.DebugString();
}

TEST(TestParserYaml, ParserErrors) {
  test_config::Config cfg;
  ParserYaml parser("/test.yml");
  const std::string input = R"(
        enabled: 1
        fieldI32: wrong
        fieldU32: {}
        fieldI64: also_wrong
        fieldU64: -64
        fieldDouble: {}
        fieldFloat: {}
        fieldString: 123
        fieldMessage: 1.2
        fieldRepeated: 1
        fieldEnum: NOT_REAL
        fieldRepeatedEnum:
            - NOT_REAL
            - ALSO_INVALID
            - TYPE2
    )";

  const std::vector<ParserError> expected = {
      "\"/test.yml\": yaml-cpp: error at line 2, column 18: bad conversion",
      "\"/test.yml\": yaml-cpp: error at line 3, column 19: bad conversion",
      "\"/test.yml\": Attempting to parse non-scalar field as scalar",
      "\"/test.yml\": yaml-cpp: error at line 5, column 19: bad conversion",
      "\"/test.yml\": yaml-cpp: error at line 6, column 19: bad conversion",
      "\"/test.yml\": Attempting to parse non-scalar field as scalar",
      "\"/test.yml\": Attempting to parse non-scalar field as scalar",
      "\"/test.yml\": Type mismatch for 'fieldMessage' - expected Map, got Scalar",
      "\"/test.yml\": Type mismatch for 'fieldRepeated' - expected Sequence, got Scalar",
      "\"/test.yml\": Invalid enum value 'NOT_REAL' for field fieldEnum",
      "\"/test.yml\": Invalid enum value 'NOT_REAL' for field fieldRepeatedEnum",
      "\"/test.yml\": Invalid enum value 'ALSO_INVALID' for field fieldRepeatedEnum",
  };

  auto errors = parser.Parse(&cfg, YAML::Load(input));
  ASSERT_TRUE(errors);
  ASSERT_EQ(errors->size(), expected.size()) << "#### parsed:" << std::endl
                                             << ErrorsToString(*errors) << std::endl
                                             << "#### expected" << std::endl
                                             << ErrorsToString(expected);

  for (unsigned int i = 0; i < expected.size(); i++) {
    ASSERT_EQ(errors->at(i), expected.at(i));
  }
}

TEST(TestParserYaml, ValidationMode) {
  test_config::Config cfg;
  struct test_case {
    ParserYaml::ValidationMode mode;
    std::string input;
    ParserResult result;
  };
  std::vector<test_case> tests = {
      {ParserYaml::STRICT,
       R"(
            asdf: asdf
            field_message: {}
        )",
       {{
           "\"/test.yml\": Missing field 'enabled'",
           "\"/test.yml\": Missing field 'field_i32'",
           "\"/test.yml\": Missing field 'field_u32'",
           "\"/test.yml\": Missing field 'field_i64'",
           "\"/test.yml\": Missing field 'field_u64'",
           "\"/test.yml\": Missing field 'field_double'",
           "\"/test.yml\": Missing field 'field_float'",
           "\"/test.yml\": Missing field 'field_string'",
           "\"/test.yml\": Missing field 'field_message.enabled'",
           "\"/test.yml\": Missing field 'field_repeated'",
           "\"/test.yml\": Missing field 'field_enum'",
           "\"/test.yml\": Missing field 'field_repeated_enum'",
           "\"/test.yml\": Unknown field 'asdf'",
       }}},
      {
          ParserYaml::STRICT,
          R"(
            enabled: true
            field_i32: -32
            field_u32: 32
            field_i64: -64
            field_u64: 64
            field_double: 3.14
            field_float: 0.12345
            field_string: Yes, this is some random string for testing
            field_message:
                enabled: true
            field_repeated:
                - 1
                - 2
                - 3
            field_enum: TYPE2
            field_repeated_enum:
                - TYPE1
                - TYPE2
            asdf: asdf
            )",
          {{
              "\"/test.yml\": Unknown field 'asdf'",
          }},
      },
      {
          ParserYaml::STRICT,
          R"(
            enabled: true
            field_i32: -32
            field_u32: 32
            field_i64: -64
            field_u64: 64
            field_double: 3.14
            field_float: 0.12345
            field_string: Yes, this is some random string for testing
            field_message:
                enabled: true
            field_repeated:
                - 1
                - 2
                - 3
            field_enum: TYPE2
            field_repeated_enum:
                - TYPE1
                - TYPE2
            )",
          {},
      },
      {
          ParserYaml::PERMISSIVE,
          R"(asdf: asdf)",
          {},
      },
      {
          ParserYaml::UNKNOWN_FIELDS_ONLY,
          R"(
            asdf: asdf
            field_message: {}
        )",
          {{"\"/test.yml\": Unknown field 'asdf'"}},
      },
  };

  for (auto& [mode, input, expected] : tests) {
    test_config::Config cfg;
    ParserYaml parser("/test.yml", false, mode);
    auto res = parser.Parse(&cfg, YAML::Load(input));
    EXPECT_EQ(res, expected) << "Mode: " << ValidationModeStr.at(mode);
  }
}

/*
 * Collector specific parsing tests
 */

TEST(CollectorConfigTest, TestYamlConfigToConfigMultiple) {
  std::vector<std::pair<std::string, Direction>> tests = {
      {R"(
                  networking:
                    externalIps:
                      enabled: enabled
               )",
       Direction::BOTH},
      {R"(
                  networking:
                    externalIps:
                      enabled: enabled
                      direction: ingress
               )",
       Direction::INGRESS},
      {R"(
                  networking:
                    externalIps:
                      enabled: enabled
                      direction: egress
               )",
       Direction::EGRESS},
      {R"(
                  networking:
                    externalIps:
                      enabled: enabled
                      direction: both
               )",
       Direction::BOTH},
      {R"(
                  networking:
                    externalIps:
                      enabled: DISABLED
               )",
       Direction::NONE},
      {R"(
                  networking:
                    externalIps:
               )",
       Direction::NONE},
      {R"(
                  networking:
               )",
       Direction::NONE},
  };

  for (const auto& [yamlStr, expected] : tests) {
    YAML::Node yamlNode = YAML::Load(yamlStr);
    CollectorConfig config;
    ASSERT_EQ(ConfigLoader(config).LoadConfiguration(yamlNode), ConfigLoader::SUCCESS) << "Input: " << yamlStr;

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_TRUE(runtime_config.has_value());

    EXPECT_EQ(config.ExternalIPsConf().GetDirection(), expected);
  }
}

TEST(CollectorConfigTest, TestYamlConfigToConfigInvalid) {
  std::vector<std::string> tests = {
      R"(
                  networking:
                    unknownField: asdf
               )",
      R"(
                  unknownField: asdf
               )"};

  for (const auto& yamlStr : tests) {
    YAML::Node yamlNode = YAML::Load(yamlStr);
    CollectorConfig config;
    ASSERT_EQ(ConfigLoader(config).LoadConfiguration(yamlNode), ConfigLoader::SUCCESS) << "Input: " << yamlStr;

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_TRUE(runtime_config.has_value()) << "Input: " << yamlStr;
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
    ASSERT_EQ(ConfigLoader(config).LoadConfiguration(yamlNode), ConfigLoader::PARSE_ERROR);

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_FALSE(runtime_config.has_value());
  }
}

TEST(CollectorConfigTest, TestMaxConnectionsPerMinute) {
  struct TestCase {
    std::string input;
    int value;
    bool valid;
    ConfigLoader::Result parse_result;
  };

  std::vector<TestCase> tests = {
      {R"(
                  networking:
                    externalIps:
                      enabled: DISABLED
                    maxConnectionsPerMinute: 1234
               )",
       1234, true, ConfigLoader::SUCCESS},
      {R"(
                  networking:
                    externalIps:
                      enabled: DISABLED
                    maxConnectionsPerMinute: 1337
               )",
       1337, true, ConfigLoader::SUCCESS},
      {R"(
                  networking:
                    externalIps:
                      enabled: DISABLED
                    maxConnectionsPerMinute: invalid
               )",
       2048, false, ConfigLoader::PARSE_ERROR},
  };

  for (const auto& [yamlStr, expected, valid, parse_result] : tests) {
    YAML::Node yamlNode = YAML::Load(yamlStr);
    CollectorConfig config;
    ASSERT_EQ(ConfigLoader(config).LoadConfiguration(yamlNode), parse_result);

    auto runtime_config = config.GetRuntimeConfig();

    EXPECT_EQ(runtime_config.has_value(), valid);

    if (valid) {
      int rate = runtime_config.value()
                     .networking()
                     .max_connections_per_minute();
      EXPECT_EQ(rate, expected);
      EXPECT_EQ(config.MaxConnectionsPerMinute(), expected);
    }
  }
}

}  // namespace collector
