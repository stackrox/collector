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

#include "CollectorStatsExporter.h"

#include <cmath>

#include "Containers.h"
#include "EventNames.h"
#include "Logging.h"
#include "SysdigService.h"
#include "Utility.h"
#include "prometheus/gauge.h"

namespace collector {

CollectorStatsExporter::CollectorStatsExporter(std::shared_ptr<prometheus::Registry> registry, const CollectorConfig* config, SysdigService* sysdig)
    : registry_(std::move(registry)), config_(config), sysdig_(sysdig) {}

bool CollectorStatsExporter::start() {
  if (!thread_.Start(&CollectorStatsExporter::run, this)) {
    CLOG(ERROR) << "Could not start stats exporter: already running";
    return false;
  }
  return true;
}

class CollectorTimerGauge {
 public:
  CollectorTimerGauge(prometheus::Family<prometheus::Gauge>& g, const std::string& timer_name)
      : events_(&g.Add({{"type", timer_name + "_events"}})),
        times_us_total_(&g.Add({{"type", timer_name + "_times_us_total"}})),
        times_us_avg_(&g.Add({{"type", timer_name + "_times_us_avg"}})) {}

  void Update(int64_t count, int64_t total_us) {
    events_->Set(count);
    times_us_total_->Set(total_us);
    times_us_avg_->Set(count ? total_us / count : 0);
  }

 private:
  prometheus::Gauge* events_;
  prometheus::Gauge* times_us_total_;
  prometheus::Gauge* times_us_avg_;
};

void CollectorStatsExporter::run() {
  auto& collectorEventCounters = prometheus::BuildGauge()
                                     .Name("rox_collector_events")
                                     .Help("Collector events")
                                     .Register(*registry_);

  auto& kernel = collectorEventCounters.Add({{"type", "kernel"}});
  auto& drops = collectorEventCounters.Add({{"type", "drops"}});
  auto& preemptions = collectorEventCounters.Add({{"type", "preemptions"}});
  auto& filtered = collectorEventCounters.Add({{"type", "filtered"}});
  auto& userspaceEvents = collectorEventCounters.Add({{"type", "userspace"}});
  auto& chiselCacheHitsAccept = collectorEventCounters.Add({{"type", "chiselCacheHitsAccept"}});
  auto& chiselCacheHitsReject = collectorEventCounters.Add({{"type", "chiselCacheHitsReject"}});
  auto& grpcSendFailures = collectorEventCounters.Add({{"type", "grpcSendFailures"}});

  auto& processSent = collectorEventCounters.Add({{"type", "processSent"}});
  auto& processSendFailures = collectorEventCounters.Add({{"type", "processSendFailures"}});
  auto& processResolutionFailuresByEvt = collectorEventCounters.Add({{"type", "processResolutionFailuresByEvt"}});
  auto& processResolutionFailuresByTinfo = collectorEventCounters.Add({{"type", "processResolutionFailuresByTinfo"}});
  auto& processRateLimitCount = collectorEventCounters.Add({{"type", "processRateLimitCount"}});

  auto& collector_timers_gauge = prometheus::BuildGauge()
                                     .Name("rox_collector_timers")
                                     .Help("Collector timers")
                                     .Register(*registry_);
  std::array<unique_ptr<CollectorTimerGauge>, CollectorStats::timer_type_max> collector_timers;
  for (int i = 0; i < CollectorStats::timer_type_max; i++) {
    auto tt = (CollectorStats::TimerType)(i);
    collector_timers[tt] = MakeUnique<CollectorTimerGauge>(collector_timers_gauge,
                                                           CollectorStats::timer_type_to_name[tt]);
  }
  auto& collector_counters_gauge = prometheus::BuildGauge()
                                       .Name("rox_collector_counters")
                                       .Help("Collector counters")
                                       .Register(*registry_);
  std::array<prometheus::Gauge*, CollectorStats::counter_type_max> collector_counters{};
  for (int i = 0; i < CollectorStats::counter_type_max; i++) {
    auto ct = (CollectorStats::CounterType)(i);
    collector_counters[ct] = &(collector_counters_gauge.Add({{"type", CollectorStats::counter_type_to_name[ct]}}));
  }

  auto& collectorTypedEventCounters = prometheus::BuildGauge()
                                          .Name("rox_collector_events_typed")
                                          .Help("Collector events by event type")
                                          .Register(*registry_);

  auto& collectorTypedEventTimesTotal = prometheus::BuildGauge()
                                            .Name("rox_collector_event_times_us_total")
                                            .Help("Collector event timings (total)")
                                            .Register(*registry_);

  auto& collectorTypedEventTimesAvg = prometheus::BuildGauge()
                                          .Name("rox_collector_event_times_us_avg")
                                          .Help("Collector event timings (average)")
                                          .Register(*registry_);

  auto& collectorProcessLineageInfo = prometheus::BuildGauge()
                                          .Name("rox_collector_process_lineage_info")
                                          .Help("Collector process lineage info")
                                          .Register(*registry_);

  prometheus::Gauge* lineage_count = &collectorProcessLineageInfo.Add({{"type", "lineage_count"}});
  prometheus::Gauge* lineage_avg = &collectorProcessLineageInfo.Add({{"type", "lineage_avg"}});
  prometheus::Gauge* lineage_std_dev = &collectorProcessLineageInfo.Add({{"type", "std_dev"}});
  prometheus::Gauge* lineage_avg_string_len = &collectorProcessLineageInfo.Add({{"type", "lineage_avg_string_len"}});

  struct {
    prometheus::Gauge* filtered = nullptr;
    prometheus::Gauge* userspace = nullptr;
    prometheus::Gauge* chiselCacheHitsAccept = nullptr;
    prometheus::Gauge* chiselCacheHitsReject = nullptr;

    prometheus::Gauge* parse_micros_total = nullptr;
    prometheus::Gauge* process_micros_total = nullptr;

    prometheus::Gauge* parse_micros_avg = nullptr;
    prometheus::Gauge* process_micros_avg = nullptr;
  } typed[PPM_EVENT_MAX] = {};

  const auto& active_syscalls = config_->Syscalls();
  UnorderedSet<std::string> syscall_set(active_syscalls.begin(), active_syscalls.end());

  const auto& event_names = EventNames::GetInstance();
  for (int i = 0; i < PPM_EVENT_MAX; i++) {
    const auto& event_name = event_names.GetEventName(i);

    if (!Contains(syscall_set, event_name)) {
      continue;
    }

    const char* event_dir = PPME_IS_ENTER(i) ? ">" : "<";

    typed[i].filtered = &collectorTypedEventCounters.Add(
        std::map<std::string, std::string>{{"quantity", "filtered"}, {"event_type", event_name}, {"event_dir", event_dir}});
    typed[i].userspace = &collectorTypedEventCounters.Add(
        std::map<std::string, std::string>{{"quantity", "userspace"}, {"event_type", event_name}, {"event_dir", event_dir}});
    typed[i].chiselCacheHitsAccept = &collectorTypedEventCounters.Add(
        std::map<std::string, std::string>{{"quantity", "chiselCacheHitsAccept"}, {"event_type", event_name}, {"event_dir", event_dir}});
    typed[i].chiselCacheHitsReject = &collectorTypedEventCounters.Add(
        std::map<std::string, std::string>{{"quantity", "chiselCacheHitsReject"}, {"event_type", event_name}, {"event_dir", event_dir}});

    typed[i].parse_micros_total = &collectorTypedEventTimesTotal.Add(
        std::map<std::string, std::string>{{"step", "parse"}, {"event_type", event_name}, {"event_dir", event_dir}});
    typed[i].process_micros_total = &collectorTypedEventTimesTotal.Add(
        std::map<std::string, std::string>{{"step", "process"}, {"event_type", event_name}, {"event_dir", event_dir}});

    typed[i].parse_micros_avg = &collectorTypedEventTimesAvg.Add(
        std::map<std::string, std::string>{{"step", "parse"}, {"event_type", event_name}, {"event_dir", event_dir}});
    typed[i].process_micros_avg = &collectorTypedEventTimesAvg.Add(
        std::map<std::string, std::string>{{"step", "process"}, {"event_type", event_name}, {"event_dir", event_dir}});
  }

  while (thread_.Pause(std::chrono::seconds(5))) {
    SysdigStats stats;
    if (!sysdig_->GetStats(&stats)) {
      continue;
    }

    kernel.Set(stats.nEvents);
    drops.Set(stats.nDrops);
    preemptions.Set(stats.nPreemptions);

    uint64_t nFiltered = 0, nUserspace = 0, nChiselCacheHitsAccept = 0, nChiselCacheHitsReject = 0;
    for (int i = 0; i < PPM_EVENT_MAX; i++) {
      auto& counters = typed[i];

      auto filtered = stats.nFilteredEvents[i];
      auto userspace = stats.nUserspaceEvents[i];
      auto chiselCacheHitsAccept = stats.nChiselCacheHitsAccept[i];
      auto chiselCacheHitsReject = stats.nChiselCacheHitsReject[i];
      auto parse_micros_total = stats.event_parse_micros[i];
      auto process_micros_total = stats.event_process_micros[i];

      nFiltered += filtered;
      nUserspace += userspace;
      nChiselCacheHitsAccept += chiselCacheHitsAccept;
      nChiselCacheHitsReject += chiselCacheHitsReject;

      if (counters.filtered) counters.filtered->Set(filtered);
      if (counters.userspace) counters.userspace->Set(userspace);
      if (counters.chiselCacheHitsAccept) counters.chiselCacheHitsAccept->Set(chiselCacheHitsAccept);
      if (counters.chiselCacheHitsReject) counters.chiselCacheHitsReject->Set(chiselCacheHitsReject);

      if (counters.parse_micros_total) counters.parse_micros_total->Set(parse_micros_total);
      if (counters.process_micros_total) counters.process_micros_total->Set(process_micros_total);

      if (counters.parse_micros_avg) counters.parse_micros_avg->Set(userspace ? parse_micros_total / userspace : 0);
      if (counters.process_micros_avg) counters.process_micros_avg->Set(filtered ? process_micros_total / filtered : 0);
    }

    filtered.Set(nFiltered);
    userspaceEvents.Set(nUserspace);
    chiselCacheHitsAccept.Set(nChiselCacheHitsAccept);
    chiselCacheHitsReject.Set(nChiselCacheHitsReject);

    grpcSendFailures.Set(stats.nGRPCSendFailures);

    // process related metrics
    processSent.Set(stats.nProcessSent);
    processSendFailures.Set(stats.nProcessSendFailures);
    processResolutionFailuresByEvt.Set(stats.nProcessResolutionFailuresByEvt);
    processResolutionFailuresByTinfo.Set(stats.nProcessResolutionFailuresByTinfo);
    processRateLimitCount.Set(stats.nProcessRateLimitCount);

    for (int i = 0; i < CollectorStats::timer_type_max; i++) {
      auto tt = (CollectorStats::TimerType)(i);
      collector_timers[tt]->Update(CollectorStats::GetOrCreate().GetTimerCount(tt),
                                   CollectorStats::GetOrCreate().GetTimerDurationMicros(tt));
    }
    for (int i = 0; i < CollectorStats::counter_type_max; i++) {
      auto ct = (CollectorStats::CounterType)(i);
      collector_counters[ct]->Set(CollectorStats::GetOrCreate().GetCounter(ct));
    }

    int64_t lineage_count_stat = CollectorStats::GetOrCreate().GetCounter(CollectorStats::process_lineage_counts);
    int64_t lineage_count_total = CollectorStats::GetOrCreate().GetCounter(CollectorStats::process_lineage_total);
    int64_t lineage_count_sqr_total = CollectorStats::GetOrCreate().GetCounter(CollectorStats::process_lineage_sqr_total);
    int64_t lineage_count_string_total = CollectorStats::GetOrCreate().GetCounter(CollectorStats::process_lineage_string_total);

    float lineage_count_avg = lineage_count_stat ? (float)lineage_count_total / (float)lineage_count_stat : 0;
    float lineage_count_sqr_avg = lineage_count_stat ? (float)lineage_count_sqr_total / (float)lineage_count_stat : 0;
    float lineage_count_std_dev = sqrt(lineage_count_sqr_avg - lineage_count_avg * lineage_count_avg);
    float lineage_count_string_avg = lineage_count_stat ? (float)lineage_count_string_total / (float)lineage_count_stat : 0;

    lineage_count->Set(lineage_count_stat);
    lineage_avg->Set(lineage_count_avg);
    lineage_std_dev->Set(lineage_count_std_dev);
    lineage_avg_string_len->Set(lineage_count_string_avg);
  }
}

void CollectorStatsExporter::stop() {
  thread_.Stop();
}

}  // namespace collector
