#ifndef SENSOR_CLIENT_FORMATTER_H
#define SENSOR_CLIENT_FORMATTER_H

#include <memory>

#include <gtest/gtest_prod.h>

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/collector_iservice.pb.h"

#include "CollectorConfig.h"
#include "ContainerMetadata.h"
#include "EventNames.h"
#include "ProtoSignalFormatter.h"

// forward definitions
class sinsp;
class sinsp_threadinfo;

namespace collector::system_inspector {
class EventExtractor;
}

namespace collector {

class SensorClientFormatter : public ProtoSignalFormatter<sensor::MsgFromCollector> {
 public:
  SensorClientFormatter(const SensorClientFormatter&) = delete;
  SensorClientFormatter(SensorClientFormatter&&) = delete;
  SensorClientFormatter& operator=(const SensorClientFormatter&) = delete;
  SensorClientFormatter& operator=(SensorClientFormatter&&) = delete;
  virtual ~SensorClientFormatter();

  SensorClientFormatter(sinsp* inspector, const CollectorConfig& config);

  using Signal = v1::Signal;
  using ProcessSignal = sensor::ProcessSignal;
  using LineageInfo = sensor::ProcessSignal_LineageInfo;
  using MsgFromCollector = sensor::MsgFromCollector;

  const MsgFromCollector* ToProtoMessage(sinsp_evt* event) override;
  const MsgFromCollector* ToProtoMessage(sinsp_threadinfo* tinfo) override;

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
  std::unique_ptr<system_inspector::EventExtractor> event_extractor_;
  ContainerMetadata container_metadata_;

  const CollectorConfig& config_;
};

}  // namespace collector

#endif  // SENSOR_CLIENT_FORMATTER_H
