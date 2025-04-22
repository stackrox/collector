#ifndef _CONFIG_LOADER_H_
#define _CONFIG_LOADER_H_

#include <optional>

#include <gtest/gtest_prod.h>
#include <yaml-cpp/yaml.h>

#include "internalapi/sensor/collector.pb.h"

#include "CollectorConfig.h"
#include "Inotify.h"
#include "utils/StoppableThread.h"

namespace collector {

class ParserError {
 public:
  ParserError() = default;
  ParserError(ParserError&&) noexcept = default;
  ParserError(const ParserError&) = default;
  ParserError& operator=(const ParserError&) = default;
  ParserError& operator=(ParserError&&) noexcept = default;
  ~ParserError() = default;

  ParserError(const char* msg) : msg_(msg) {}
  ParserError(std::string msg) : msg_(std::move(msg)) {}

  const std::string& What() const { return msg_; }

  template <typename T>
  friend ParserError& operator<<(ParserError& e, const T& msg) {
    std::stringstream ss;
    ss << msg;
    e.msg_ += ss.str();
    return e;
  }

  friend std::ostream& operator<<(std::ostream& os, const ParserError& err) {
    os << err.What();
    return os;
  }

  friend bool operator==(const ParserError& lhs, const ParserError& rhs) { return lhs.msg_ == rhs.msg_; }

 private:
  std::string msg_;
};

using ParserResult = std::optional<std::vector<ParserError>>;

class ParserYaml {
 public:
  enum ValidationMode : uint8_t {
    STRICT = 0,           ///< Fail on unknown or missing fields
    PERMISSIVE,           ///< No failures for missing or unknown fields
    UNKNOWN_FIELDS_ONLY,  ///< Fail on unknown fields, but allow missing fields
  };

  ParserYaml(std::filesystem::path file, bool read_camelcase = true, ValidationMode v = PERMISSIVE)
      : file_(std::move(file)), read_camelcase_(read_camelcase), validation_mode_(v) {}

  /**
   * Populate a protobuf message from the configuration file assigned
   * to this parser.
   *
   * @param msg The protobuf message to be populated.
   * @returns an optional vector of parser errors.
   */
  ParserResult Parse(google::protobuf::Message* msg);

  /**
   * Populate a protobuf message from a provided YAML::Node.
   *
   * @param msg The protobuf message to be populated.
   * @param node A YAML::Node used to populate the message.
   * @returns an optional vector of parser errors.
   */
  ParserResult Parse(google::protobuf::Message* msg, const YAML::Node& node);

  const std::filesystem::path& GetFile() { return file_; }

 private:
  /**
   * Inner method that will parse the provided field into the protobuf
   * message from the corresponding values in the YAML::Node.
   *
   * @param msg The protobuf message to be populated.
   * @param node A YAML::Node used to populate the message.
   * @param field The descriptor for the field being parsed.
   * @param path A string showing the full path to the current field.
   * @returns an optional vector of parser errors.
   */
  ParserResult Parse(google::protobuf::Message* msg, const YAML::Node& node,
                     const google::protobuf::FieldDescriptor* field, const std::string& path);

  /**
   * Populate a repeated protobuf message from an array.
   *
   * @param msg The protobuf message to be populated.
   * @param node A YAML::Node used to populate the message.
   * @param field The descriptor for the field being parsed.
   * @returns an optional vector of parser errors.
   */
  ParserResult ParseArray(google::protobuf::Message* msg, const YAML::Node& node,
                          const google::protobuf::FieldDescriptor* field);

  /**
   * Populate a repeated protobuf message from an array.
   *
   * @param msg The protobuf message to be populated.
   * @param node A YAML::Node used to populate the message.
   * @param field The descriptor for the field being parsed.
   * @returns an optional vector of parser errors.
   */
  template <typename T>
  ParserResult ParseArrayInner(google::protobuf::Message* msg, const YAML::Node& node,
                               const google::protobuf::FieldDescriptor* field);

  /**
   * Populate a repeated enum field from an array.
   *
   * @param msg The protobuf message to be populated.
   * @param node A YAML::Node used to populate the message.
   * @param field The descriptor for the field being parsed.
   * @returns an optional vector of parser errors.
   */
  ParserResult ParseArrayEnum(google::protobuf::Message* msg, const YAML::Node& node,
                              const google::protobuf::FieldDescriptor* field);

  ParserResult ParseScalar(google::protobuf::Message* msg, const YAML::Node& node,
                           const google::protobuf::FieldDescriptor* field, const std::string& name);
  /**
   * Go through all nodes in the configuration and notify of any
   * elements that have no corresponding field in the protobuf message.
   *
   * @param msg The protobuf message used for validation.
   * @param node The YAML::Node to be walked.
   * @returns an optional vector of parser errors.
   */
  ParserResult FindUnknownFields(const google::protobuf::Message& msg, const YAML::Node& node);

  ParserError WrapError(const std::exception& e);

  /**
   * Read a value from a YAML::Node, preventing exceptions from being
   * thrown.
   *
   * @param node A YAML::Node to be read.
   * @returns Either the read value or a parser error.
   */
  template <typename T>
  std::variant<T, ParserError> TryConvert(const YAML::Node& node);

  /**
   * Check if the result of TryConvert is an error.
   *
   * @param res The output from a call to TryConvert
   * @returns true if a parsing error occurred, false otherwise.
   */
  template <typename T>
  static bool IsError(const std::variant<T, ParserError>& res) {
    return std::holds_alternative<ParserError>(res);
  }

  static std::string SnakeCaseToCamel(const std::string& s);
  static std::string CamelCaseToSnake(const std::string& s);

  std::filesystem::path file_;
  bool read_camelcase_;
  ValidationMode validation_mode_;
};

/**
 * Reload configuration based on inotify events received on a
 * configuration file.
 */
class ConfigLoader {
 public:
  ConfigLoader() = delete;
  ConfigLoader(CollectorConfig& config);

  void Start();
  void Stop();

  enum Result : uint8_t {
    SUCCESS = 0,
    PARSE_ERROR,
    FILE_NOT_FOUND,
  };

  /**
   * Load a configuration file into the supplied CollectorConfig object.
   *
   * Alternatively, a YAML::Node can be supplied to load the
   * configuration from it. This is mostly meant for testing pusposes.
   *
   * @param node a YAML::Node to be used as configuration
   * @returns true if configuration loading was successful.
   */
  Result LoadConfiguration(const std::optional<const YAML::Node>& node = std::nullopt);

 private:
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigMultiple);
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigInvalid);
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigEmptyOrMalformed);
  FRIEND_TEST(CollectorConfigTest, TestMaxConnectionsPerMinute);

  /**
   * Create a new runtime configuration object with correct defaults.
   *
   * @returns The new runtime configuration object.
   */
  static sensor::CollectorConfig NewRuntimeConfig();

  /**
   * Wait for inotify events on a configuration file and reload it
   * accordingly.
   */
  void WatchFile();

  /**
   * Handle an inotify event, updating the configuration if needed.
   *
   * @param event The inotify event to be processed
   * @returns false if the directory holding the configuration file is
   *          removed or renamed, true otherwise.
   */
  bool HandleEvent(const struct inotify_event* event);

  /**
   * Handle an inotify event on the directory holding the configuration
   * file and updating the configuration if needed.
   *
   * @param event The inotify event to be processed
   * @returns false if the directory holding the configuration file is
   *          removed or renamed, true otherwise.
   */
  bool HandleConfigDirectoryEvent(const struct inotify_event* event);

  /**
   * Handle an inotify event on the configuration file, updating the
   * configuration if needed.
   *
   * @param event The inotify event to be processed
   * @param w An iterator to the Watcher for the configuration file.
   */
  void HandleConfigFileEvent(const struct inotify_event* event, Inotify::WatcherIterator w);

  /**
   * Handle an inotify event on the configuration file, updating the
   * configuration if needed.
   *
   * This method is used in case the configuration file is a symlink
   * and the inotify event corresponds to the hardlink file that is
   * being pointed at.
   *
   * @param event The inotify event to be processed
   * @param w An iterator to the Watcher for the configuration file.
   */
  void HandleConfigRealpathEvent(const struct inotify_event* event, Inotify::WatcherIterator w);

  CollectorConfig& config_;
  Inotify inotify_;
  StoppableThread thread_;
  ParserYaml parser_;
};

}  // namespace collector

#endif  // _CONFIG_LOADER_H_
