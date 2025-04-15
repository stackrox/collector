#pragma once

#include <memory>

#include <gtest/gtest_prod.h>

#include "api/v1/signal.pb.h"

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

class SensorClientFormatter : public ProtoSignalFormatter<sensor::ProcessSignal> {
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

  const ProcessSignal* ToProtoMessage(sinsp_evt* event) override;
  const ProcessSignal* ToProtoMessage(sinsp_threadinfo* tinfo) override;

  /**
   * Get the list of processes that spawned the current one.
   *
   * The list will be limited to processes inside the same container as
   * the one being processed.
   *
   * @param tinfo The sinsp_threadinfo for which we should generate
   *              lineage.
   * @returns A vector with the lineage information.
   */
  static std::vector<LineageInfo> GetProcessLineage(sinsp_threadinfo* tinfo);

 private:
  FRIEND_TEST(SensorClientFormatterTest, NoProcessArguments);
  FRIEND_TEST(SensorClientFormatterTest, ProcessArguments);

  /**
   * Allocate and fill in a ProcessSignal message.
   *
   * @param event A Falco event to be translated into a ProcessSignal.
   * @returns A non-owning pointer to the ProcessSignal.
   */
  ProcessSignal* CreateProcessSignal(sinsp_evt* event);

  /**
   * Check if the provided threadinfo has the required fields.
   *
   * @param tinfo the Falco threadinfo to validate.
   * @returns true if the threadinfo is valid, false otherwise.
   */
  bool ValidateProcessDetails(const sinsp_threadinfo* tinfo);

  /**
   * Check if the provided event has the required fields.
   *
   * @param event the Falco event to validate.
   * @returns true if the threadinfo is valid, false otherwise.
   */
  bool ValidateProcessDetails(sinsp_evt* event);

  /**
   * Translate a Falco event to a printable string.
   *
   * @param event The Falco event to be translated.
   * @returns A printable string.
   */
  std::string ToString(sinsp_evt* event);

  /**
   * Allocate and fill in a ProcessSignal message.
   *
   * @param tinfo A Falco threadinfo to be translated into a ProcessSignal.
   * @returns A non-owning pointer to the ProcessSignal.
   */
  ProcessSignal* CreateProcessSignal(sinsp_threadinfo* tinfo);

  /**
   * Update lineage related prometheus stats.
   *
   * @param lineage The lineage used for updating the stats.
   */
  static void UpdateLineageStats(const std::vector<LineageInfo>& lineage);

  const EventNames& event_names_;
  std::unique_ptr<system_inspector::EventExtractor> event_extractor_;
  ContainerMetadata container_metadata_;

  const CollectorConfig& config_;
};

}  // namespace collector
