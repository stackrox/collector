#include "ConfigLoader.h"

#include <google/protobuf/descriptor.h>

#include "internalapi/sensor/collector.pb.h"

#include "EnvVar.h"
#include "Logging.h"

namespace collector {

namespace {
const PathEnvVar CONFIG_FILE("ROX_COLLECTOR_CONFIG_PATH", "/etc/stackrox/runtime_config.yaml");

enum PathTags {
  LOADER_PARENT_PATH = 1,
  LOADER_CONFIG_FILE,
  LOADER_CONFIG_REALPATH,
};

std::string NodeTypeToString(YAML::NodeType::value type) {
  // Don't add the default case so linters can warn about a missing type
  switch (type) {
    case YAML::NodeType::Null:
      return "Null";
    case YAML::NodeType::Undefined:
      return "Undefined";
    case YAML::NodeType::Scalar:
      return "Scalar";
    case YAML::NodeType::Sequence:
      return "Sequence";
    case YAML::NodeType::Map:
      return "Map";
  }
  return "";  // Unreachable
}
};  // namespace

namespace stdf = std::filesystem;

ParserResult ParserYaml::Parse(google::protobuf::Message* msg) {
  YAML::Node node;
  try {
    node = YAML::LoadFile(file_);
  } catch (const YAML::BadFile& e) {
    return {{WrapError(e)}};
  } catch (const YAML::ParserException& e) {
    return {{WrapError(e)}};
  }

  return Parse(msg, node);
}

ParserResult ParserYaml::Parse(google::protobuf::Message* msg, const YAML::Node& node) {
  using namespace google::protobuf;

  if (node.IsScalar() || node.IsNull()) {
    return {{"Invalid configuration"}};
  }

  std::vector<ParserError> errors;

  const Descriptor* descriptor = msg->GetDescriptor();
  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor* field = descriptor->field(i);

    auto err = Parse(msg, node, field);
    if (err) {
      errors.insert(errors.end(), err->begin(), err->end());
    }
  }

  auto res = FindUnkownFields(*msg, node);
  if (res) {
    errors.insert(errors.end(), res->begin(), res->end());
  }

  if (!errors.empty()) {
    return errors;
  }

  return {};
}

template <typename T>
ParserResult ParserYaml::ParseArrayInner(google::protobuf::Message* msg, const YAML::Node& node,
                                         const google::protobuf::FieldDescriptor* field) {
  std::vector<ParserError> errors;
  auto f = msg->GetReflection()->GetMutableRepeatedFieldRef<T>(msg, field);
  f.Clear();
  for (const auto& n : node) {
    auto value = TryConvert<T>(n);
    if (!IsError(value)) {
      f.Add(std::get<T>(value));
    } else {
      errors.emplace_back(std::get<ParserError>(value));
    }
  }

  if (!errors.empty()) {
    return errors;
  }
  return {};
}

ParserResult ParserYaml::ParseArrayEnum(google::protobuf::Message* msg, const YAML::Node& node,
                                        const google::protobuf::FieldDescriptor* field) {
  using namespace google::protobuf;

  std::unique_ptr<std::string> name_ptr = nullptr;
  const std::string* name = &field->name();

  if (read_camelcase_) {
    name_ptr = std::make_unique<std::string>(SnakeCaseToCamel(*name));
    name = name_ptr.get();
  }

  std::vector<ParserError> errors;
  auto f = msg->GetReflection()->GetMutableRepeatedFieldRef<int32>(msg, field);
  f.Clear();

  const EnumDescriptor* desc = field->enum_type();
  for (const auto& n : node) {
    auto v = TryConvert<std::string_view>(n);
    if (IsError(v)) {
      errors.emplace_back(std::get<ParserError>(v));
      continue;
    }
    const auto enum_name = std::get<std::string_view>(v);

    const EnumValueDescriptor* value = desc->FindValueByName(enum_name);
    if (value == nullptr) {
      ParserError err;
      err << file_ << ": Invalid enum value '" << enum_name << "' for field " << *name;
      errors.emplace_back(err);
      continue;
    }

    f.Add(value->number());
  }

  if (!errors.empty()) {
    return errors;
  }
  return {};
}

ParserResult ParserYaml::FindUnkownFields(const google::protobuf::Message& msg, const YAML::Node& node) {
  using namespace google::protobuf;

  const auto* descriptor = msg.GetDescriptor();
  std::vector<ParserError> errors;

  for (YAML::const_iterator it = node.begin(); it != node.end(); it++) {
    auto name = it->first.as<std::string>();
    if (read_camelcase_) {
      name = CamelCaseToSnake(name);
    }

    const FieldDescriptor* field = descriptor->FindFieldByName(name);
    if (field == nullptr) {
      std::stringstream ss;
      ss << "Unknown field '" << name << "'";
      errors.emplace_back(ss.str());
      continue;
    }

    if (it->second.IsMap()) {
      if (field->type() != FieldDescriptor::TYPE_MESSAGE) {
        std::stringstream ss;
        ss << file_ << ": Invalid type '" << NodeTypeToString(it->second.Type()) << "' for field " << it->first.as<std::string_view>() << ", expected '" << field->type_name() << "'";
        errors.emplace_back(ss.str());
        continue;
      }

      const auto* reflection = msg.GetReflection();
      auto res = FindUnkownFields(reflection->GetMessage(msg, field), it->second);

      if (res) {
        errors.insert(errors.end(), res->begin(), res->end());
      }
    }
  }

  if (!errors.empty()) {
    return errors;
  }
  return {};
}

ParserResult ParserYaml::Parse(google::protobuf::Message* msg, const YAML::Node& node,
                               const google::protobuf::FieldDescriptor* field) {
  using namespace google::protobuf;

  std::unique_ptr<std::string> name_ptr = nullptr;
  const std::string* name = &field->name();
  if (read_camelcase_) {
    name_ptr = std::make_unique<std::string>(SnakeCaseToCamel(*name));
    name = name_ptr.get();
  }

  if (!node[*name]) {
    return {};
  }

  if (field->label() == FieldDescriptor::LABEL_REPEATED) {
    if (!node[*name].IsSequence()) {
      ParserError err;
      YAML::NodeType::value type = node[*name].Type();
      err << file_ << ": Type mismatch for '" << *name << "' - expected Sequence, got "
          << NodeTypeToString(type);
      return {{err}};
    }
    return ParseArray(msg, node[*name], field);
  }

  if (field->type() == FieldDescriptor::TYPE_MESSAGE) {
    if (node[*name].IsNull()) {
      // Ignore empty objects
      return {};
    }

    if (!node[*name].IsMap()) {
      ParserError err;
      YAML::NodeType::value type = node[*name].Type();
      err << file_ << ": Type mismatch for '" << *name << "' - expected Map, got "
          << NodeTypeToString(type);
      return {{err}};
    }
    std::vector<ParserError> errors;
    const Reflection* reflection = msg->GetReflection();

    Message* m = reflection->MutableMessage(msg, field);

    const Descriptor* descriptor = m->GetDescriptor();
    for (int i = 0; i < descriptor->field_count(); i++) {
      const FieldDescriptor* f = descriptor->field(i);

      auto err = Parse(m, node[*name], f);
      if (err) {
        errors.insert(errors.end(), err->begin(), err->end());
      }
    }

    if (!errors.empty()) {
      return errors;
    }
    return {};
  }

  if (!node[*name].IsScalar()) {
    ParserError err;
    err << file_ << ": Attempting to parse non-scalar field as scalar";
    return {{err}};
  }

  switch (field->type()) {
    case FieldDescriptor::TYPE_DOUBLE: {
      auto value = TryConvert<double>(node[*name]);
      if (IsError(value)) {
        return {{std::get<ParserError>(value)}};
      }

      msg->GetReflection()->SetDouble(msg, field, std::get<double>(value));
    } break;
    case FieldDescriptor::TYPE_FLOAT: {
      auto value = TryConvert<float>(node[*name]);
      if (IsError(value)) {
        return {{std::get<ParserError>(value)}};
      }

      msg->GetReflection()->SetFloat(msg, field, std::get<float>(value));
    } break;
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_INT64: {
      auto value = TryConvert<int64_t>(node[*name]);
      if (IsError(value)) {
        return {{std::get<ParserError>(value)}};
      }

      msg->GetReflection()->SetInt64(msg, field, std::get<int64_t>(value));
    } break;
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_UINT64: {
      auto value = TryConvert<uint64_t>(node[*name]);
      if (IsError(value)) {
        return {{std::get<ParserError>(value)}};
      }

      msg->GetReflection()->SetUInt64(msg, field, std::get<uint64_t>(value));
    } break;
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_UINT32: {
      auto value = TryConvert<uint32_t>(node[*name]);
      if (IsError(value)) {
        return {{std::get<ParserError>(value)}};
      }

      msg->GetReflection()->SetUInt32(msg, field, std::get<uint32_t>(value));
    } break;
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_INT32: {
      auto value = TryConvert<int32_t>(node[*name]);
      if (IsError(value)) {
        return {{std::get<ParserError>(value)}};
      }

      msg->GetReflection()->SetInt32(msg, field, std::get<int32_t>(value));
    } break;
    case FieldDescriptor::TYPE_BOOL: {
      auto value = TryConvert<bool>(node[*name]);
      if (IsError(value)) {
        return {{std::get<ParserError>(value)}};
      }

      msg->GetReflection()->SetBool(msg, field, std::get<bool>(value));

    } break;
    case FieldDescriptor::TYPE_STRING: {
      auto value = TryConvert<std::string>(node[*name]);
      if (IsError(value)) {
        return {{std::get<ParserError>(value)}};
      }

      msg->GetReflection()->SetString(msg, field, std::get<std::string>(value));

    } break;
    case FieldDescriptor::TYPE_BYTES:
      std::cerr << "Unsupported type BYTES" << std::endl;
      break;
    case FieldDescriptor::TYPE_ENUM: {
      const auto enum_name = node[*name].as<std::string_view>();

      const EnumDescriptor* descriptor = field->enum_type();
      const EnumValueDescriptor* value = descriptor->FindValueByName(enum_name);
      if (value == nullptr) {
        ParserError err;
        err << file_ << ": Invalid enum value '" << enum_name << "' for field " << *name;
        return {{err}};
      }
      msg->GetReflection()->SetEnumValue(msg, field, value->number());
    } break;

    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP: {
      ParserError err;
      err << "Unexpected type: " << field->type_name();
      return {{err}};
    }
  }

  return {};
}

ParserResult ParserYaml::ParseArray(google::protobuf::Message* msg, const YAML::Node& node,
                                    const google::protobuf::FieldDescriptor* field) {
  using namespace google::protobuf;

  // mapping for repeated fields:
  // https://protobuf.dev/reference/cpp/api-docs/google.protobuf.message/#Reflection.GetRepeatedFieldRef.details
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return ParseArrayInner<int32>(msg, node, field);
    case FieldDescriptor::CPPTYPE_UINT32:
      return ParseArrayInner<uint32_t>(msg, node, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ParseArrayInner<int64_t>(msg, node, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ParseArrayInner<uint64_t>(msg, node, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ParseArrayInner<double>(msg, node, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ParseArrayInner<float>(msg, node, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ParseArrayInner<bool>(msg, node, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      return ParseArrayEnum(msg, node, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return ParseArrayInner<std::string>(msg, node, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
      return {{"Unsupport repeated type MESSAGE"}};
    } break;
    default: {
      std::stringstream ss;
      ss << "Unknown type " << field->type_name();
      return {{ss.str()}};
    }
  }
}

ParserError ParserYaml::WrapError(const std::exception& e) {
  std::stringstream ss;
  ss << file_ << ": " << e.what();
  return ss.str();
}

template <typename T>
std::variant<T, ParserError> ParserYaml::TryConvert(const YAML::Node& node) {
  try {
    return node.as<T>();
  } catch (YAML::InvalidNode& e) {
    return WrapError(e);
  } catch (YAML::BadConversion& e) {
    return WrapError(e);
  }
}

std::string ParserYaml::SnakeCaseToCamel(const std::string& s) {
  std::string out;
  bool capitalize = false;

  for (const auto& c : s) {
    if (c == '_') {
      capitalize = true;
      continue;
    }

    if (capitalize) {
      out += (char)std::toupper(c);
    } else {
      out += c;
    }
    capitalize = false;
  }

  return out;
}

std::string ParserYaml::CamelCaseToSnake(const std::string& s) {
  std::string out;
  bool first = true;

  for (const auto& c : s) {
    if (!first && std::isupper(c) != 0) {
      out += '_';
    }
    out += (char)std::tolower(c);
    first = false;
  }

  return out;
}

ConfigLoader::ConfigLoader(CollectorConfig& config)
    : config_(config), parser_(CONFIG_FILE.value()) {}

void ConfigLoader::Start() {
  thread_.Start([this] { WatchFile(); });
  CLOG(INFO) << "Watching configuration file: " << parser_.GetFile().string();
}

void ConfigLoader::Stop() {
  thread_.Stop();
  CLOG(INFO) << "No longer watching configuration file: " << parser_.GetFile().string();
}

ConfigLoader::Result ConfigLoader::LoadConfiguration(const std::optional<const YAML::Node>& node) {
  sensor::CollectorConfig runtime_config;
  ParserResult errors;

  if (!node.has_value()) {
    if (!stdf::exists(parser_.GetFile())) {
      return FILE_NOT_FOUND;
    }
    errors = parser_.Parse(&runtime_config);

  } else {
    errors = parser_.Parse(&runtime_config, *node);
  }

  if (errors) {
    CLOG(ERROR) << "Failed to parse " << parser_.GetFile();
    for (const auto& err : *errors) {
      CLOG(ERROR) << err;
    }
    return PARSE_ERROR;
  }

  config_.SetRuntimeConfig(std::move(runtime_config));
  CLOG(DEBUG) << "Runtime configuration:\n"
              << config_.GetRuntimeConfigStr();
  return SUCCESS;
}

void ConfigLoader::WatchFile() {
  const auto& file = parser_.GetFile();

  if (!inotify_.IsValid()) {
    CLOG(ERROR) << "Configuration reloading will not be used for " << file;
    return;
  }

  if (inotify_.AddDirectoryWatcher(file.parent_path(), LOADER_PARENT_PATH) < 0) {
    return;
  }

  if (stdf::exists(file)) {
    inotify_.AddFileWatcher(file, LOADER_CONFIG_FILE);

    if (stdf::is_symlink(file)) {
      inotify_.AddFileWatcher(stdf::canonical(file), LOADER_CONFIG_REALPATH);
    }

    // Reload configuration in case it has changed since startup
    LoadConfiguration();
  }

  while (!thread_.should_stop()) {
    InotifyResult res = inotify_.GetNext();

    switch (res.index()) {
      case INOTIFY_OK:
        if (!HandleEvent(std::get<const inotify_event*>(res))) {
          // Error during handling of reload, stopping thread
          return;
        }
        break;
      case INOTIFY_ERROR:
        CLOG_THROTTLED(WARNING, std::chrono::seconds(30)) << std::get<InotifyError>(res).what();
        break;
      case INOTIFY_TIMEOUT:
        // Nothing to do
        break;
    }
  }
}

bool ConfigLoader::HandleEvent(const struct inotify_event* event) {
  if ((event->mask & IN_Q_OVERFLOW) != 0) {
    CLOG(WARNING) << "inotify events have been dropped";
  }

  auto w = inotify_.FindWatcher(event->wd);
  if (inotify_.WatcherNotFound(w)) {
    // Log a warning only if it is not an ignore event
    bool is_ignored = (event->mask & IN_IGNORED) != 0;
    CLOG_IF(!is_ignored, WARNING) << "Failed to find watcher for inotify event: " << event->wd
                                  << " - mask: [ " << Inotify::MaskToString(event->mask) << " ]";
    return true;
  }

  switch (w->tag) {
    case LOADER_PARENT_PATH:
      return HandleConfigDirectoryEvent(event);
    case LOADER_CONFIG_FILE:
      HandleConfigFileEvent(event, w);
      break;
    case LOADER_CONFIG_REALPATH:
      HandleConfigRealpathEvent(event, w);
      break;
  }
  return true;
}

bool ConfigLoader::HandleConfigDirectoryEvent(const struct inotify_event* event) {
  const auto& file = parser_.GetFile();

  CLOG(DEBUG) << "Got directory event for " << file.parent_path() / event->name << " - mask: [ " << Inotify::MaskToString(event->mask) << " ]";

  if ((event->mask & (IN_MOVE_SELF | IN_DELETE_SELF)) != 0) {
    CLOG(ERROR) << "Configuration directory was removed or renamed. Stopping runtime configuration";
    return false;
  }

  if (file.filename() != event->name) {
    return true;
  }

  if ((event->mask & (IN_CREATE | IN_MOVED_TO)) != 0) {
    inotify_.AddFileWatcher(file, LOADER_CONFIG_FILE);
    LoadConfiguration();
    if (stdf::is_symlink(file)) {
      inotify_.AddFileWatcher(stdf::canonical(file), LOADER_CONFIG_REALPATH);
    }
  } else if ((event->mask & (IN_DELETE | IN_MOVED_FROM)) != 0) {
    auto w = inotify_.FindWatcher(file);
    inotify_.RemoveWatcher(w);
    config_.ResetRuntimeConfig();
  }

  return true;
}

void ConfigLoader::HandleConfigFileEvent(const struct inotify_event* event, Inotify::WatcherIterator w) {
  // in this method w->path == file_ == config_file
  CLOG(DEBUG) << "Got event for " << w->path << " - mask: [ " << Inotify::MaskToString(event->mask) << " ]";
  if ((event->mask & IN_MODIFY) != 0) {
    if (stdf::is_symlink(w->path)) {
      // File is a symlink and was modified, add a watcher to the
      // newly pointed file.
      inotify_.AddFileWatcher(stdf::canonical(w->path), LOADER_CONFIG_REALPATH);
    }
    LoadConfiguration();
  } else if ((event->mask & (IN_MOVE_SELF | IN_DELETE_SELF)) != 0) {
    if (stdf::exists(w->path)) {
      inotify_.AddFileWatcher(w->path, LOADER_CONFIG_FILE);
      if (stdf::is_symlink(w->path)) {
        // The new configuration file is actually a symlink,
        // add a watcher to the pointed file.
        inotify_.AddFileWatcher(stdf::canonical(w->path), LOADER_CONFIG_REALPATH);
      }
      LoadConfiguration();
    } else {
      inotify_.RemoveWatcher(w);
      config_.ResetRuntimeConfig();
    }
  }
}

void ConfigLoader::HandleConfigRealpathEvent(const struct inotify_event* event, Inotify::WatcherIterator w) {
  CLOG(DEBUG) << "Got realpath event for " << w->path << " - mask: [ " << Inotify::MaskToString(event->mask) << " ]";
  if ((event->mask & IN_MODIFY) != 0) {
    LoadConfiguration();
  } else if ((event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) != 0) {
    const auto& file = parser_.GetFile();
    // If the original file was a symlink pointing to this file and
    // it still exists, we need to add a new watcher to the newly
    // pointed configuration file and reload the configuration.
    if (stdf::is_symlink(file)) {
      inotify_.AddFileWatcher(stdf::canonical(file), LOADER_CONFIG_REALPATH);
      LoadConfiguration();
    } else {
      inotify_.RemoveWatcher(w);
      config_.ResetRuntimeConfig();
    }
  }
}

}  // namespace collector
