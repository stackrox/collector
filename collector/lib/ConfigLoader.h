#ifndef _CONFIG_LOADER_H_
#define _CONFIG_LOADER_H_

#include <gtest/gtest_prod.h>
#include <yaml-cpp/yaml.h>

#include "CollectorConfig.h"
#include "Inotify.h"
#include "StoppableThread.h"

namespace collector {

class ParserError {
 public:
  ParserError() = default;
  ParserError(ParserError&&) noexcept = default;
  ParserError(const ParserError&) = default;
  ParserError& operator=(const ParserError&) = default;
  ParserError& operator=(ParserError&&) noexcept = default;
  ~ParserError() = default;

  ParserError(const char* msg) { msg_ += msg; }
  ParserError(const std::string& msg) { msg_ += msg; }

  const std::string& What() const { return msg_; }

  template <typename T>
  friend ParserError& operator<<(ParserError& e, const T msg) {
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

using ParserErrors = std::vector<ParserError>;
using ParserResult = std::optional<ParserErrors>;

class ParserYaml {
 public:
  ParserYaml(std::filesystem::path file, bool read_camelcase = true) : file_(std::move(file)), read_camelcase_(read_camelcase) {}

  ParserResult Parse(google::protobuf::Message* msg);
  ParserResult Parse(google::protobuf::Message* msg, const YAML::Node& node);

  const std::filesystem::path& GetFile() { return file_; }

 private:
  ParserResult Parse(google::protobuf::Message* msg, const YAML::Node& node,
                     const google::protobuf::FieldDescriptor* field);
  ParserResult ParseArray(google::protobuf::Message* msg, const YAML::Node& node,
                          const google::protobuf::FieldDescriptor* field);
  template <typename T>
  ParserResult ParseArrayInner(google::protobuf::Message* msg, const YAML::Node& node,
                               const google::protobuf::FieldDescriptor* field);
  ParserResult ParseArrayEnum(google::protobuf::Message* msg, const YAML::Node& node,
                              const google::protobuf::FieldDescriptor* field);
  ParserResult FindUnkownFields(const google::protobuf::Message& msg, const YAML::Node& node);

  ParserError WrapError(const std::exception& e);

  template <typename T>
  std::variant<T, ParserError> TryConvert(const YAML::Node& node);
  template <typename T>
  static bool IsError(const std::variant<T, ParserError>& res) {
    return std::holds_alternative<ParserError>(res);
  }
  static std::string SnakeCaseToCamel(const std::string& s);
  static std::string CamelCaseToSnake(const std::string& s);

  std::filesystem::path file_;
  bool read_camelcase_;
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

  /**
   * Load a configuration file into the supplied CollectorConfig object.
   *
   * @returns true if configuration loading was successful.
   */
  bool LoadConfiguration();

 private:
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigMultiple);
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigInvalid);
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigEmptyOrMalformed);
  FRIEND_TEST(CollectorConfigTest, TestMaxConnectionsPerMinute);

  /**
   * Load configuration from a YAML string.
   *
   * This method is meant to be used for testing only.
   *
   * @param node a YAML::Node with the new configuration to be used.
   * @returns true if configuration loading was successful.
   */
  bool LoadConfiguration(const YAML::Node& node);

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
