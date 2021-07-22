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

#ifndef _PROCESS_SIGNAL_FORMATTER_H_
#define _PROCESS_SIGNAL_FORMATTER_H_

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/signal_iservice.pb.h"
#include "storage/process_indicator.pb.h"

#include "EventNames.h"
#include "ProtoSignalFormatter.h"
#include "SysdigEventExtractor.h"

namespace collector {

class CollectorStats;

class ProcessSignalFormatter : public ProtoSignalFormatter<sensor::SignalStreamMessage> {
 public:
  ProcessSignalFormatter(sinsp* inspector) : event_names_(EventNames::GetInstance()) {
    event_extractor_.Init(inspector);
  }

  using Signal = v1::Signal;
  using ProcessSignal = storage::ProcessSignal;
  using LineageInfo = storage::ProcessSignal_LineageInfo;

  const sensor::SignalStreamMessage* ToProtoMessage(sinsp_evt* event) override;
  const sensor::SignalStreamMessage* ToProtoMessage(sinsp_threadinfo* tinfo);

  void GetProcessLineage(sinsp_threadinfo* tinfo, std::vector<LineageInfo>& lineage);
  void SetCollectorStats(CollectorStats* stats) { stats_ = stats; }

 private:
  Signal* CreateSignal(sinsp_evt* event);
  ProcessSignal* CreateProcessSignal(sinsp_evt* event);
  bool ValidateProcessDetails(sinsp_evt* event);
  std::string ProcessDetails(sinsp_evt* event);

  Signal* CreateSignal(sinsp_threadinfo* tinfo);
  ProcessSignal* CreateProcessSignal(sinsp_threadinfo* tinfo);
  bool ValidateProcessDetails(sinsp_threadinfo* tinfo);

  const EventNames& event_names_;
  SysdigEventExtractor event_extractor_;
  CollectorStats* stats_ = nullptr;
};

}  // namespace collector

#endif  // _PROCESS_SIGNAL_FORMATTER_H_
