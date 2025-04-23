#pragma once

#include <atomic>
#include <cstdint>

#include "CollectorConfig.h"
#include "Control.h"
#include "ppm_events_public.h"

namespace collector::system_inspector {

struct Stats {
  using uint64_t = std::uint64_t;

  // stats gathered in kernel space
  volatile uint64_t nEvents = 0;       // the number of kernel events
  volatile uint64_t nDrops = 0;        // the number of drops
  volatile uint64_t nDropsBuffer = 0;  // the number of drops due to full ringbuf
  volatile uint64_t nPreemptions = 0;  // the number of preemptions

  // stats gathered in user space
  volatile uint64_t nFilteredEvents[PPM_EVENT_MAX] = {0};   // events post filtering
  volatile uint64_t nUserspaceEvents[PPM_EVENT_MAX] = {0};  // events processed by userspace
  volatile uint64_t nGRPCSendFailures = 0;                  // number of signals that were not sent on GRPC
  volatile uint64_t nThreadCacheSize = 0;                   // number of thread-info entries stored in the cache
  volatile uint64_t nDropsThreadCache = 0;                  // the number of drops due to full thread cache

  // process related metrics
  volatile uint64_t nProcessSent = 0;                       // number of process signals sent
  volatile uint64_t nProcessSendFailures = 0;               // number of process signals failed to send
  volatile uint64_t nProcessResolutionFailuresByEvt = 0;    // number of process signals failed to resolve by event*
  volatile uint64_t nProcessResolutionFailuresByTinfo = 0;  // number of process signals failed to resolve by tinfo*
  volatile uint64_t nProcessRateLimitCount = 0;             // number of process signals rate limited

  // Timing metrics
  volatile uint64_t event_parse_micros[PPM_EVENT_MAX] = {0};    // total microseconds spent parsing event type (correlates w/ nUserspaceEvents)
  volatile uint64_t event_process_micros[PPM_EVENT_MAX] = {0};  // total microseconds spent processing event type (correlates w/ nFilteredevents)
};

class SystemInspector {
 public:
  SystemInspector() = default;
  SystemInspector(const SystemInspector&) = default;
  SystemInspector(SystemInspector&&) = delete;
  SystemInspector& operator=(const SystemInspector&) = default;
  SystemInspector& operator=(SystemInspector&&) = delete;
  virtual ~SystemInspector() = default;

  virtual bool InitKernel(const CollectorConfig& config) = 0;
  virtual void Start() = 0;
  virtual void Run(const std::atomic<ControlValue>& control) = 0;
  virtual void CleanUp() = 0;

  virtual bool GetStats(Stats* stats) const = 0;
};

}  // namespace collector::system_inspector
