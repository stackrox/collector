#ifndef _SYSDIG_
#define _SYSDIG_

#include <atomic>
#include <cstdint>
#include <string>

#include "CollectorService.h"
#include "ConnTracker.h"
#include "ppm_events_public.h"

namespace collector {

struct SysdigStats {
  using uint64_t = std::uint64_t;

  // stats gathered in kernel space
  volatile uint64_t nEvents = 0;       // the number of kernel events
  volatile uint64_t nDrops = 0;        // the number of drops
  volatile uint64_t nPreemptions = 0;  // the number of preemptions

  // stats gathered in user space
  volatile uint64_t nFilteredEvents[PPM_EVENT_MAX] = {0};         // events post chisel filter
  volatile uint64_t nUserspaceEvents[PPM_EVENT_MAX] = {0};        // events pre chisel filter, should be (nEvents - nDrops)
  volatile uint64_t nChiselCacheHitsAccept[PPM_EVENT_MAX] = {0};  // number of events that hit the filter cache
  volatile uint64_t nChiselCacheHitsReject[PPM_EVENT_MAX] = {0};  // number of events that hit the filter cache
  volatile uint64_t nGRPCSendFailures = 0;                        // number of signals that were not sent on GRPC

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

class Sysdig {
 public:
  virtual ~Sysdig() = default;

  virtual void Init(const CollectorConfig& config, std::shared_ptr<ConnectionTracker> conn_tracker) = 0;
  virtual void Start() = 0;
  virtual void Run(const std::atomic<CollectorService::ControlValue>& control) = 0;
  virtual void CleanUp() = 0;

  virtual bool GetStats(SysdigStats* stats) const = 0;
};

} /* namespace collector */

#endif /* _SYSDIG_ */
