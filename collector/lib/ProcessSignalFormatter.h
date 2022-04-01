#ifndef _PROCESS_SIGNAL_FORMATTER_H_
#define _PROCESS_SIGNAL_FORMATTER_H_

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/signal_iservice.pb.h"
#include "storage/process_indicator.pb.h"

#include "CollectorStats.h"
#include "EventNames.h"
#include "ProtoSignalFormatter.h"
#include "SysdigEventExtractor.h"

namespace collector {

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

 private:
  Signal* CreateSignal(sinsp_evt* event);
  ProcessSignal* CreateProcessSignal(sinsp_evt* event);
  bool ValidateProcessDetails(sinsp_evt* event);
  std::string ProcessDetails(sinsp_evt* event);

  Signal* CreateSignal(sinsp_threadinfo* tinfo);
  ProcessSignal* CreateProcessSignal(sinsp_threadinfo* tinfo);
  bool ValidateProcessDetails(sinsp_threadinfo* tinfo);
  int GetTotalStringLength(const std::vector<LineageInfo>& lineage);
  void CountLineage(const std::vector<LineageInfo>& lineage);

  const EventNames& event_names_;
  SysdigEventExtractor event_extractor_;
  //CollectorStats* stats_ = CollectorStats::GetOrCreate();
};

}  // namespace collector

#endif  // _PROCESS_SIGNAL_FORMATTER_H_
