#ifndef COLLECTOR_PROCFSSCRAPER_H
#define COLLECTOR_PROCFSSCRAPER_H

#include <cstring>
#include <string>
#include <vector>

#include "NetworkConnection.h"

namespace collector {

// Abstract interface for a ConnScraper. Useful to inject testing implementation.
class IConnScraper {
 public:
  virtual bool Scrape(std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints, std::string tcp_file_path) = 0;
  virtual ~IConnScraper() {}
};

// ConnScraper is a class that allows scraping a `/proc`-like directory structure for active network connections.
class ConnScraper : public IConnScraper {
 public:
  explicit ConnScraper(std::string proc_path, std::shared_ptr<ProcessStore> process_store = 0)
      : proc_path_(std::move(proc_path)),
        process_store_(process_store) {}

  // Scrape returns a snapshot of all active network connections in the given vector.
  bool Scrape(std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints, std::string tcp_file_path);

 private:
  std::string proc_path_;
  std::shared_ptr<ProcessStore> process_store_;
};

class ProcessScraper {
 public:
  ProcessScraper(std::string proc_path) : proc_path_(std::move(proc_path)) {}

  class ProcessInfo {
   public:
    std::string container_id;
    std::string comm;      // binary name
    std::string exe;       // argv[0]
    std::string exe_path;  // full binary path
    std::string args;      // space separated concatenation of arguments
    uint64_t pid;
  };

  bool Scrape(uint64_t pid, ProcessInfo& pi);

 private:
  std::string proc_path_;
};

}  // namespace collector

#endif  // COLLECTOR_CONNSCRAPER_H
