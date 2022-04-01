#ifndef COLLECTOR_CONNSCRAPER_H
#define COLLECTOR_CONNSCRAPER_H

#include <cstring>
#include <string>
#include <vector>

#include "FileSystem.h"
#include "Hash.h"
#include "NetworkConnection.h"
#include "StringView.h"

namespace collector {

// ExtractContainerID tries to extract a container ID from a cgroup line. Exposed for testing.
StringView ExtractContainerID(StringView cgroup_line);

// ConnScraper is a class that allows scraping a `/proc`-like directory structure for active network connections.
class ConnScraper {
 public:
  explicit ConnScraper(std::string proc_path) : proc_path_(std::move(proc_path)) {}

  // Scrape returns a snapshot of all active network connections in the given vector.
  bool Scrape(std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints);

 private:
  std::string proc_path_;
};

}  // namespace collector

#endif  //COLLECTOR_CONNSCRAPER_H
