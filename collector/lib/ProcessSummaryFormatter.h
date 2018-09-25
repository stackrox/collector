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

#ifndef _PROCESS_SUMMARY_FORMATTER_H_
#define _PROCESS_SUMMARY_FORMATTER_H_

#include "EventNames.h"
#include "ProtoSignalFormatter.h"
#include "SysdigEventExtractor.h"

#include "data/process_summary.pb.h"

namespace collector {

class ProcessSummaryFormatter : public ProtoSignalFormatter<data::ProcessSummary> {
 public:
  ProcessSummaryFormatter(sinsp* inspector, const uuid_t* cluster_id, bool text_format = false)
      : ProtoSignalFormatter(text_format), event_names_(EventNames::GetInstance()),
        cluster_id_(cluster_id) {
    event_extractor_.Init(inspector);
  }

  using ProcessSummary = data::ProcessSummary;
  using ProcessOp = ProcessSummary::ProcessOp;
  using ProcessOperation = ProcessSummary::ProcessOperation;
  using ProcessOperationDetails = ProcessOperation::Details;
  using SocketDetails = ProcessOperationDetails::Socket;
  using ModuleDetails = ProcessOperationDetails::Module;
  using PermissionChangeDetails = ProcessOperationDetails::PermissionChange;
  using ProcessContainer = ProcessSummary::Container;
  using ProcessDetails = data::Process;

  const data::ProcessSummary* ToProtoMessage(sinsp_evt* event) override;

 private:
  ProcessOperation* CreateProcessOperation(sinsp_evt* event, ProcessOp op);
  ProcessOperationDetails* CreateProcessOperationDetails(sinsp_evt* event, ProcessOp op);
  SocketDetails* CreateSocketDetails(sinsp_evt* event);
  ModuleDetails* CreateModuleDetails(sinsp_evt* event);
  PermissionChangeDetails* CreatePermissionChangeDetails(sinsp_evt* event);
  ProcessContainer* CreateProcessContainer(sinsp_evt* event);
  ProcessDetails* CreateProcessDetails(sinsp_evt* event);

  const EventNames& event_names_;
  SysdigEventExtractor event_extractor_;
  const uuid_t* cluster_id_;
};

}  // namespace collector

#endif  // _PROCESS_SUMMARY_FORMATTER_H_
