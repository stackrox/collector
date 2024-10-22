#ifndef _CONFIG_LOADER_H_
#define _CONFIG_LOADER_H_

#include <yaml-cpp/yaml.h>

#include "CollectorConfig.h"
#include "Inotify.h"
#include "StoppableThread.h"

namespace collector {

class ConfigLoader {
 public:
  ConfigLoader() = delete;
  ConfigLoader(CollectorConfig& config);

  void Start();
  void Stop();
  static bool LoadConfiguration(CollectorConfig& config);
  static bool LoadConfiguration(CollectorConfig& config, const YAML::Node& node);

 private:
  void WatchFile();
  bool HandleEvent(const struct inotify_event* event);
  bool HandleConfigDirectoryEvent(const struct inotify_event* event);
  void HandleConfigFileEvent(const struct inotify_event* event, Inotify::WatcherIterator w);
  void HandleConfigRealpathEvent(const struct inotify_event* event, Inotify::WatcherIterator w);
  bool LoadConfiguration() {
    return LoadConfiguration(config_);
  }

  CollectorConfig& config_;
  Inotify inotify_;
  StoppableThread thread_;
  const std::filesystem::path& file_;
};

}  // namespace collector

#endif  // _CONFIG_LOADER_H_
