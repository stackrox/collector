#ifndef _CONFIG_LOADER_H_
#define _CONFIG_LOADER_H_

#include <yaml-cpp/yaml.h>

#include "CollectorConfig.h"
#include "Inotify.h"
#include "StoppableThread.h"

namespace collector {

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
   * @param config The target object for the loaded configuration.
   * @returns true if configuration loading was successful.
   */
  static bool LoadConfiguration(CollectorConfig& config);

  /**
   * Load a configuration file into the supplied CollectorConfig object
   * from the provided yaml node.
   *
   * @param config The target object for the loaded configuration.
   * @param node a YAML::Node holding the new configuration.
   * @returns true if configuration loading was successful.
   */
  static bool LoadConfiguration(CollectorConfig& config, const YAML::Node& node);

 private:
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
