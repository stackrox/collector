/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

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
  virtual bool Scrape(std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints) = 0;
  virtual ~IConnScraper() {}
};

// ConnScraper is a class that allows scraping a `/proc`-like directory structure for active network connections.
class ConnScraper : public IConnScraper {
 public:
  explicit ConnScraper(std::string proc_path, bool are_udp_listening_endpoints_collected, std::shared_ptr<ProcessStore> process_store = 0)
      : proc_path_(std::move(proc_path)), are_udp_listening_endpoints_collected_(are_udp_listening_endpoints_collected), process_store_(process_store) {}

  // Scrape returns a snapshot of all active network connections in the given vector.
  bool Scrape(std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints);

 private:
  std::string proc_path_;
  bool are_udp_listening_endpoints_collected_;
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
