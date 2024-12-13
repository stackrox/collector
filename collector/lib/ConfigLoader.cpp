#include "ConfigLoader.h"

#include <google/protobuf/util/json_util.h>

#include <internalapi/sensor/collector.pb.h>

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
}  // namespace

namespace stdf = std::filesystem;

ConfigLoader::ConfigLoader(CollectorConfig& config)
    : config_(config), file_(CONFIG_FILE.value()) {}

void ConfigLoader::Start() {
  thread_.Start([this] { WatchFile(); });
  CLOG(INFO) << "Watching configuration file: " << file_;
}

void ConfigLoader::Stop() {
  thread_.Stop();
  CLOG(INFO) << "No longer watching configuration file: " << file_;
}

bool ConfigLoader::LoadConfiguration(CollectorConfig& config) {
  const auto& config_file = CONFIG_FILE.value();
  YAML::Node node;

  if (!stdf::exists(config_file)) {
    CLOG(DEBUG) << "No configuration file found: " << config_file;
    return true;
  }

  try {
    node = YAML::LoadFile(config_file);
  } catch (const YAML::BadFile& e) {
    CLOG(ERROR) << "Failed to open the configuration file: " << config_file << ". Error: " << e.what();
    return false;
  } catch (const YAML::ParserException& e) {
    CLOG(ERROR) << "Failed to parse the configuration file: " << config_file << ". Error: " << e.what();
    return false;
  }

  return LoadConfiguration(config, node);
}

Json::Value yamlNodeToJson(const YAML::Node& yamlNode) {
  Json::Value jsonValue;

  switch (yamlNode.Type()) {
    case YAML::NodeType::Null:
      jsonValue = Json::Value::null;
      break;
    case YAML::NodeType::Scalar:
      jsonValue = yamlNode.as<std::string>();
      break;
    case YAML::NodeType::Sequence:
      for (std::size_t i = 0; i < yamlNode.size(); ++i) {
        jsonValue.append(yamlNodeToJson(yamlNode[i]));
      }
      break;
    case YAML::NodeType::Map:
      for (const auto& pair : yamlNode) {
        jsonValue[pair.first.as<std::string>()] = yamlNodeToJson(pair.second);
      }
      break;
    case YAML::NodeType::Undefined:
    default:
      break;
  }

  return jsonValue;
}

bool ConfigLoader::LoadConfiguration(CollectorConfig& config, const YAML::Node& node) {
  const auto jsonConfig = yamlNodeToJson(node);
  Json::StreamWriterBuilder writer;
  std::string jsonStr = Json::writeString(writer, jsonConfig);

  sensor::CollectorConfig runtimeConfig;
  google::protobuf::util::JsonParseOptions parseOptions;
  parseOptions.ignore_unknown_fields = true;
  auto status = google::protobuf::util::JsonStringToMessage(jsonStr, &runtimeConfig, parseOptions);
  if (!status.ok()) {
    CLOG(ERROR) << "Failed to parse config: " << status.ToString();
    return false;
  }

  config.SetRuntimeConfig(runtimeConfig);

  return true;
}

void ConfigLoader::WatchFile() {
  if (!inotify_.IsValid()) {
    CLOG(ERROR) << "Configuration reloading will not be used for " << file_;
    return;
  }

  if (inotify_.AddDirectoryWatcher(file_.parent_path(), LOADER_PARENT_PATH) < 0) {
    return;
  }

  if (stdf::exists(file_)) {
    inotify_.AddFileWatcher(file_, LOADER_CONFIG_FILE);

    if (stdf::is_symlink(file_)) {
      inotify_.AddFileWatcher(stdf::canonical(file_), LOADER_CONFIG_REALPATH);
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
  CLOG(DEBUG) << "Got directory event for " << file_.parent_path() / event->name << " - mask: [ " << Inotify::MaskToString(event->mask) << " ]";

  if ((event->mask & (IN_MOVE_SELF | IN_DELETE_SELF)) != 0) {
    CLOG(ERROR) << "Configuration directory was removed or renamed. Stopping runtime configuration";
    return false;
  }

  if (file_.filename() != event->name) {
    return true;
  }

  if ((event->mask & (IN_CREATE | IN_MOVED_TO)) != 0) {
    inotify_.AddFileWatcher(file_, LOADER_CONFIG_FILE);
    LoadConfiguration();
    if (stdf::is_symlink(file_)) {
      inotify_.AddFileWatcher(stdf::canonical(file_), LOADER_CONFIG_REALPATH);
    }
  } else if ((event->mask & (IN_DELETE | IN_MOVED_FROM)) != 0) {
    auto w = inotify_.FindWatcher(file_);
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
    // If the original file was a symlink pointing to this file and
    // it still exists, we need to add a new watcher to the newly
    // pointed configuration file and reload the configuration.
    if (stdf::is_symlink(file_)) {
      inotify_.AddFileWatcher(stdf::canonical(file_), LOADER_CONFIG_REALPATH);
      LoadConfiguration();
    } else {
      inotify_.RemoveWatcher(w);
      config_.ResetRuntimeConfig();
    }
  }
}

}  // namespace collector
