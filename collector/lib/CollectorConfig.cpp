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

#include <sstream>

#include "CollectorConfig.h"
#include "CollectorArgs.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

const bool kDefaultUseChiselCache = true;
const bool kDefaultSnapLen = 0;
const int kDefaultScrapeInterval = 30;
const bool kDefaultTurnOffScrape = false;

const std::string kDefaultCollectionMethod = "kernel-module";

const std::vector<std::string> kDefaultSyscalls = 
  {"accept","connect","execve","fork","clone","close","shutdown","socket"};

const char* kDefaultChisel = R"(
-- Chisel description
description = "only display events relevant to security modeling and detection"
short_description = "security relevant"
category = "misc"

-- Chisel argument list
args = {}

-- Event parsing callback
function on_event()
    return true
end

function on_init()
    filter = "not container.id = 'host'\n"
    chisel.set_filter(filter)
    return true
end

)";

CollectorConfig::CollectorConfig(CollectorArgs *args) {
  // Default values
  use_chisel_cache_ = kDefaultUseChiselCache;
  scrape_interval_ = kDefaultScrapeInterval;
  turn_off_scrape_ = kDefaultTurnOffScrape;
  snap_len_ = kDefaultSnapLen;
  chisel_ = kDefaultChisel;
  syscalls_ = kDefaultSyscalls;
  collection_method_ = kDefaultCollectionMethod;
  hostname_ = GetHostname();
  host_proc_ = GetHostPath("/proc");

  // Check user provided configuration
  if (args) {
    auto config = args->CollectorConfig();

    // Chisel Cache
    if (!config["useChiselCache"].empty()) {
      use_chisel_cache_ = config["useChiselCache"].asBool();
      CLOG(INFO) << "User configured useChiselCache=" << use_chisel_cache_;
    }

    // Scrape Interval
    if (!config["scrapeInterval"].empty()) {
      scrape_interval_ = std::stoi(config["scrapeInterval"].asString());
      CLOG(INFO) << "User configured scrapeInterval=" << scrape_interval_;
    }

    // Scrape Enabled/Disabled 
    if (!config["turnOffScrape"].empty()) {
      turn_off_scrape_ = config["turnOffScrape"].asBool();
      CLOG(INFO) << "User configured turnOffScrape=" << turn_off_scrape_;
    }

    // Chisel
    if (args->Chisel().length()) {
      auto chiselB64 = args->Chisel();
      CLOG(INFO) << "User configured chisel=" << chiselB64;
      chisel_ = Base64Decode(chiselB64);
    }

    // Syscalls
    if (!config["syscalls"].empty() && config["syscalls"].isArray()) {
      auto syscall_list = config["syscalls"];
      std::vector<std::string> syscalls;
      for (const auto& syscall_json : syscall_list) {
        syscalls.push_back(syscall_json.asString());
      }
      syscalls_ = syscalls;

      std::stringstream ss;
      for (auto& s : syscalls_) ss << s << ",";
      CLOG(INFO) << "User configured syscalls=" << ss.str();

    }

    // Collection Method
    if (args->CollectionMethod().length() > 0) {
      collection_method_ = args->CollectionMethod();
      CLOG(INFO) << "User configured collection-method=" << collection_method_;
    }

    // Ebpf (deprecated)
    if (!config["useEbpf"].empty()) {
      collection_method_ = config["useEbpf"].asBool() ? "ebpf" : "kernel_module";
      CLOG(INFO) << "User configured useEbpf=" << config["useEbpf"].asBool();
    }

  }
}

bool CollectorConfig::UseChiselCache() const {
  return use_chisel_cache_;
}

bool CollectorConfig::UseEbpf() const {
  return (collection_method_ == "ebpf");
}

bool CollectorConfig::TurnOffScrape() const {
  return turn_off_scrape_;
}

int CollectorConfig::ScrapeInterval() const {
  return scrape_interval_;
}

int CollectorConfig::SnapLen() const {
  return snap_len_;
}

std::string CollectorConfig::Chisel() const {
  return chisel_;
}

std::string CollectorConfig::CollectionMethod() const {
  return collection_method_;
}

std::string CollectorConfig::Hostname() const {
  return hostname_;
}

std::string CollectorConfig::HostProc() const {
  return host_proc_;
}

std::vector<std::string> CollectorConfig::Syscalls() const {
  return syscalls_;
}

std::string CollectorConfig::asString() const {
  std::stringstream ss;
  ss << "collection_method:" << this->collection_method_;
  ss << ", useChiselCache:" << this->use_chisel_cache_;
  ss << ", snapLen:" << this->snap_len_;
  ss << ", scrape_interval:" << this->scrape_interval_;
  ss << ", turn_off_scrape:" << this->turn_off_scrape_;
  ss << ", hostname:" << this->hostname_;
  return ss.str();
}

}  // namespace collector
