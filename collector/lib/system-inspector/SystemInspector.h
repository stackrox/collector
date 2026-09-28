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

  // libsinsp fdtable/threadtable state counters (from sinsp_stats_v2)
  volatile uint64_t nFdCacheHits = 0;       // cached fd lookups
  volatile uint64_t nFdCacheMisses = 0;     // non-cached fd lookups
  volatile uint64_t nFdLookupFailures = 0;  // failed fd lookups
  volatile uint64_t nFdsAdded = 0;          // fds added to fdtables
  volatile uint64_t nFdsRemoved = 0;        // fds removed from fdtables

  volatile uint64_t nThreadCacheHits = 0;       // cached thread lookups
  volatile uint64_t nThreadCacheMisses = 0;     // non-cached thread lookups
  volatile uint64_t nThreadLookupFailures = 0;  // failed thread lookups
  volatile uint64_t nThreadsAdded = 0;          // threads added to the thread table
  volatile uint64_t nThreadsRemoved = 0;        // threads removed from the thread table

  // number of times a thread lookup fell back to a direct /proc scan (sinsp_thread_manager)
  volatile uint64_t nProcLookups = 0;            // total /proc lookups performed
  volatile uint64_t nMainThreadLookups = 0;      // /proc lookups performed specifically for main threads
  volatile uint64_t nProcLookupsDurationNs = 0;  // total time spent performing /proc lookups

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
