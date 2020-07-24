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

#ifndef _COLLECTOR_CONFIG_H_
#define _COLLECTOR_CONFIG_H_

#include <ostream>
#include <vector>

#include <grpcpp/channel.h>

namespace collector {

class CollectorArgs;

class CollectorConfig {
 public:
  static constexpr bool        kUseChiselCache = true;
  static constexpr bool        kSnapLen = 0;
  static constexpr bool        kTurnOffScrape = false;
  static constexpr int         kScrapeInterval = 30;
  static constexpr char        kCollectionMethod[] = "kernel-module";
  static constexpr const char* kSyscalls[] = {"accept","connect","execve","fork","clone","close","shutdown","socket","procexit","procinfo", "chdir", "fchdir"};
  static constexpr char        kChisel[] = R"(
args = {}
function on_event()
    return true
end
function on_init()
    filter = "not container.id = 'host'\n"
    chisel.set_filter(filter)
    return true
end
)";

  CollectorConfig() = delete;
  CollectorConfig(CollectorArgs* collectorArgs);

  std::string asString() const;

  bool UseEbpf() const;
  bool UseChiselCache() const;
  bool TurnOffScrape() const;
  int ScrapeInterval() const;
  int SnapLen() const;
  std::string Chisel() const;
  std::string Hostname() const;
  std::string HostProc() const;
  std::string CollectionMethod() const;
  std::vector<std::string> Syscalls() const;
  std::string LogLevel() const;
  bool EnableSysdigLog() const { return enable_sysdig_log_; }
  bool DisableNetworkFlows() const { return disable_network_flows_; }

  std::shared_ptr<grpc::Channel> grpc_channel;

 private: 
  bool use_chisel_cache_;
  int scrape_interval_;
  int snap_len_;
  std::string collection_method_;
  std::string chisel_;
  bool turn_off_scrape_;
  std::vector<std::string> syscalls_;
  std::string hostname_;
  std::string host_proc_;
  bool disable_network_flows_ = false;

  bool enable_sysdig_log_ = false;
};

std::ostream& operator<< (std::ostream& os, const CollectorConfig& c);

} // end namespace collector

#endif  // _COLLECTOR_CONFIG_H_
