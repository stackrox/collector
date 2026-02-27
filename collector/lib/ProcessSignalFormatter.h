#pragma once

#include <memory>

#include <gtest/gtest_prod.h>

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/signal_iservice.pb.h"
#include "storage/process_indicator.pb.h"

#include "CollectorConfig.h"
#include "CollectorStats.h"
#include "ContainerMetadata.h"
#include "EventNames.h"
#include "ProtoSignalFormatter.h"

// forward definitions
class sinsp;
class sinsp_threadinfo;
namespace collector {
namespace system_inspector {
class EventExtractor;
}
}  // namespace collector

namespace collector {

class ProcessSignalFormatter : public ProtoSignalFormatter<sensor::SignalStreamMessage> {
 public:
  ProcessSignalFormatter(sinsp* inspector, const CollectorConfig& config);
  ~ProcessSignalFormatter();

  using Signal = v1::Signal;
  using ProcessSignal = storage::ProcessSignal;
  using LineageInfo = storage::ProcessSignal_LineageInfo;

  const sensor::SignalStreamMessage* ToProtoMessage(sinsp_evt* event) override;
  const sensor::SignalStreamMessage* ToProtoMessage(sinsp_threadinfo* tinfo);

  void GetProcessLineage(sinsp_threadinfo* tinfo, std::vector<LineageInfo>& lineage);

 private:
  FRIEND_TEST(ProcessSignalFormatterTest, NoProcessArguments);
  FRIEND_TEST(ProcessSignalFormatterTest, ProcessArguments);

  ProcessSignal* CreateProcessSignal(sinsp_evt* event);
  Signal* CreateSignal(sinsp_evt* event);
  bool ValidateProcessDetails(const sinsp_threadinfo* tinfo);
  bool ValidateProcessDetails(sinsp_evt* event);
  std::string ProcessDetails(sinsp_evt* event);

  Signal* CreateSignal(sinsp_threadinfo* tinfo);
  ProcessSignal* CreateProcessSignal(sinsp_threadinfo* tinfo);
  int GetTotalStringLength(const std::vector<LineageInfo>& lineage);
  void CountLineage(const std::vector<LineageInfo>& lineage);

  const EventNames& event_names_;
  sinsp* inspector_;
  std::unique_ptr<system_inspector::EventExtractor> event_extractor_;
  ContainerMetadata container_metadata_;

  const CollectorConfig& config_;
};

}  // namespace collector
