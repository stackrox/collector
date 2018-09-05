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

#ifndef _SYSDIG_SERVICE_H_
#define _SYSDIG_SERVICE_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "libsinsp/sinsp.h"
#include "libsinsp/chisel.h"

#include "CollectorService.h"
#include "EventClassifier.h"
#include "KafkaClient.h"
#include "SafeBuffer.h"
#include "SignalWriter.h"
#include "Sysdig.h"
#include "SignalFormatter.h"

namespace collector {

class SysdigService : public Sysdig {
 public:
  static constexpr char kModulePath[] = "/module/collector.ko";
  static constexpr char kModuleName[] = "collector";
  static constexpr int kMessageBufferSize = 8192;
  static constexpr int kKeyBufferSize = 48;

  SysdigService() = default;

  void Init(const CollectorConfig& config) override;
  void Start() override;
  void Run(const std::atomic<CollectorService::ControlValue>& control) override;
  void SetChisel(const std::string& new_chisel);
  void CleanUp() override;

  bool GetStats(SysdigStats* stats) const override;

 private:
  enum ChiselCacheStatus : int {
    BLOCKED_USERSPACE,
    BLOCKED_KERNEL,
    ACCEPTED,
  };

  SignalType GetNext(SafeBuffer* message_buffer, SafeBuffer* key_buffer);

  bool FilterEvent(sinsp_evt* event);
  bool SendExistingProcesses();

  std::unique_ptr<sinsp> inspector_;
  std::unique_ptr<sinsp_chisel> chisel_;
  std::unique_ptr<SignalWriter> signal_writers_[SIGNAL_TYPE_MAX + 1];
  std::unique_ptr<SignalFormatter> signal_formatter_[SIGNAL_TYPE_MAX + 1];
  EventClassifier classifier_;
  SysdigStats userspace_stats_;

  std::unordered_map<string, ChiselCacheStatus> chisel_cache_;
  bool use_chisel_cache_;

  mutable std::mutex running_mutex_;
  bool running_ = false;
};

}  // namespace collector

#endif  // _SYSDIG_SERVICE_H_
