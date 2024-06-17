#include "CollectorStatsExporter.h"

#include <chrono>
#include <iostream>
#include <math.h>

#include "Containers.h"
#include "EventNames.h"
#include "Logging.h"
#include "Utility.h"
#include "prometheus/gauge.h"
#include "prometheus/summary.h"
#include "system-inspector/Service.h"

namespace collector {

template <typename T>
class CollectorConnectionStatsPrometheus : public CollectorConnectionStats<T> {
 public:
  CollectorConnectionStatsPrometheus(
      std::shared_ptr<prometheus::Registry> registry,
      std::string name,
      std::string help,
      std::chrono::milliseconds max_age,
      const std::vector<double>& quantiles,
      double error) : family_(prometheus::BuildSummary().Name(name).Help(help).Register(*registry)),
                      inbound_private_summary_(family_.Add({{"dir", "in"}, {"peer", "private"}}, MakeQuantiles(quantiles, error), max_age)),
                      inbound_public_summary_(family_.Add({{"dir", "in"}, {"peer", "public"}}, MakeQuantiles(quantiles, error), max_age)),
                      outbound_private_summary_(family_.Add({{"dir", "out"}, {"peer", "private"}}, MakeQuantiles(quantiles, error), max_age)),
                      outbound_public_summary_(family_.Add({{"dir", "out"}, {"peer", "public"}}, MakeQuantiles(quantiles, error), max_age)) {}

  void Observe(T inbound_private, T inbound_public, T outbound_private, T outbound_public) override {
    inbound_private_summary_.Observe(inbound_private);
    inbound_public_summary_.Observe(inbound_public);
    outbound_private_summary_.Observe(outbound_private);
    outbound_public_summary_.Observe(outbound_public);
  }

 private:
  prometheus::Family<prometheus::Summary>& family_;
  prometheus::Summary& inbound_private_summary_;
  prometheus::Summary& inbound_public_summary_;
  prometheus::Summary& outbound_private_summary_;
  prometheus::Summary& outbound_public_summary_;

  prometheus::Summary::Quantiles MakeQuantiles(std::vector<double> quantiles, double error) {
    prometheus::Summary::Quantiles result;

    result.reserve(quantiles.size());

    auto make_quantile = [error](double q) -> prometheus::detail::CKMSQuantiles::Quantile {
      return {q, error};
    };

    std::transform(quantiles.begin(), quantiles.end(), std::back_inserter(result), make_quantile);

    return result;
  }
};

CollectorStatsExporter::CollectorStatsExporter(std::shared_ptr<prometheus::Registry> registry, const CollectorConfig* config, system_inspector::Service* si)
    : registry_(std::move(registry)),
      config_(config),
      system_inspector_(si),
      connections_total_reporter_(std::make_shared<CollectorConnectionStatsPrometheus<unsigned int>>(
          registry_,
          "rox_connections_total",
          "Amount of stored connections over time",
          std::chrono::minutes{config->GetConnectionStatsWindow()},
          config->GetConnectionStatsQuantiles(),
          config->GetConnectionStatsError())),
      connections_rate_reporter_(std::make_shared<CollectorConnectionStatsPrometheus<float>>(
          registry_,
          "rox_connections_rate",
          "Rate of connections over time",
          std::chrono::minutes{config->GetConnectionStatsWindow()},
          config->GetConnectionStatsQuantiles(),
          config->GetConnectionStatsError())) {}

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
  auto& threadCacheDrops = collectorEventCounters.Add({{"type", "threadCacheDrops"}});
  auto& ringbufferDrops = collectorEventCounters.Add({{"type", "ringbufferDrops"}});
  auto& preemptions = collectorEventCounters.Add({{"type", "preemptions"}});
  auto& grpcSendFailures = collectorEventCounters.Add({{"type", "grpcSendFailures"}});
  auto& threadTableSize = collectorEventCounters.Add({{"type", "threadCacheSize"}});

  auto& processSent = collectorEventCounters.Add({{"type", "processSent"}});
  auto& processSendFailures = collectorEventCounters.Add({{"type", "processSendFailures"}});
  auto& processResolutionFailuresByEvt = collectorEventCounters.Add({{"type", "processResolutionFailuresByEvt"}});
  auto& processResolutionFailuresByTinfo = collectorEventCounters.Add({{"type", "processResolutionFailuresByTinfo"}});
  auto& processRateLimitCount = collectorEventCounters.Add({{"type", "processRateLimitCount"}});

  auto& collector_timers_gauge = prometheus::BuildGauge()
                                     .Name("rox_collector_timers")
                                     .Help("Collector timers")
                                     .Register(*registry_);
  std::array<std::unique_ptr<CollectorTimerGauge>, CollectorStats::timer_type_max> collector_timers;
  for (int i = 0; i < CollectorStats::timer_type_max; i++) {
    auto tt = (CollectorStats::TimerType)(i);
    collector_timers[tt] = MakeUnique<CollectorTimerGauge>(collector_timers_gauge,
                                                           CollectorStats::timer_type_to_name[tt]);
  }
  auto& collector_counters_gauge = prometheus::BuildGauge()
                                       .Name("rox_collector_counters")
                                       .Help("Collector counters")
                                       .Register(*registry_);
  std::array<prometheus::Gauge*, CollectorStats::counter_type_max> collector_counters;
  for (int i = 0; i < CollectorStats::counter_type_max; i++) {
    auto ct = (CollectorStats::CounterType)(i);
    collector_counters[ct] = &(collector_counters_gauge.Add({{"type", CollectorStats::counter_type_to_name[ct]}}));
  }

  auto& collectorProcessLineageInfo = prometheus::BuildGauge()
                                          .Name("rox_collector_process_lineage_info")
                                          .Help("Collector process lineage info")
                                          .Register(*registry_);

  prometheus::Gauge* lineage_count = &collectorProcessLineageInfo.Add({{"type", "lineage_count"}});
  prometheus::Gauge* lineage_avg = &collectorProcessLineageInfo.Add({{"type", "lineage_avg"}});
  prometheus::Gauge* lineage_std_dev = &collectorProcessLineageInfo.Add({{"type", "std_dev"}});
  prometheus::Gauge* lineage_avg_string_len = &collectorProcessLineageInfo.Add({{"type", "lineage_avg_string_len"}});

  // Following counters are used for detailed metrics
  prometheus::Gauge* userspaceEvents = nullptr;
  prometheus::Family<prometheus::Gauge>* collectorTypedEventCounters = nullptr;
  prometheus::Family<prometheus::Gauge>* collectorTypedEventTimesTotal = nullptr;
  prometheus::Family<prometheus::Gauge>* collectorTypedEventTimesAvg = nullptr;

  struct {
    prometheus::Gauge* userspace = nullptr;

    prometheus::Gauge* parse_micros_total = nullptr;
    prometheus::Gauge* process_micros_total = nullptr;

    prometheus::Gauge* parse_micros_avg = nullptr;
    prometheus::Gauge* process_micros_avg = nullptr;
  } typed[PPM_EVENT_MAX] = {};

  const auto& active_syscalls = config_->Syscalls();
  UnorderedSet<std::string> syscall_set(active_syscalls.begin(), active_syscalls.end());

  const auto& event_names = EventNames::GetInstance();

  if (config_->EnableDetailedMetrics()) {
    userspaceEvents = &collectorEventCounters.Add({{"type", "userspace"}});

    collectorTypedEventCounters = &prometheus::BuildGauge()
                                       .Name("rox_collector_events_typed")
                                       .Help("Collector events by event type")
                                       .Register(*registry_);

    collectorTypedEventTimesTotal = &prometheus::BuildGauge()
                                         .Name("rox_collector_event_times_us_total")
                                         .Help("Collector event timings (total)")
                                         .Register(*registry_);

    collectorTypedEventTimesAvg = &prometheus::BuildGauge()
                                       .Name("rox_collector_event_times_us_avg")
                                       .Help("Collector event timings (average)")
                                       .Register(*registry_);

    for (int i = 0; i < PPM_EVENT_MAX; i++) {
      const auto& event_name = event_names.GetEventName(i);

      if (!Contains(syscall_set, event_name)) {
        continue;
      }

      const char* event_dir = PPME_IS_ENTER(i) ? ">" : "<";

      typed[i].userspace = &collectorTypedEventCounters->Add(
          std::map<std::string, std::string>{{"quantity", "userspace"}, {"event_type", event_name}, {"event_dir", event_dir}});

      typed[i].parse_micros_total = &collectorTypedEventTimesTotal->Add(
          std::map<std::string, std::string>{{"step", "parse"}, {"event_type", event_name}, {"event_dir", event_dir}});
      typed[i].process_micros_total = &collectorTypedEventTimesTotal->Add(
          std::map<std::string, std::string>{{"step", "process"}, {"event_type", event_name}, {"event_dir", event_dir}});

      typed[i].parse_micros_avg = &collectorTypedEventTimesAvg->Add(
          std::map<std::string, std::string>{{"step", "parse"}, {"event_type", event_name}, {"event_dir", event_dir}});
      typed[i].process_micros_avg = &collectorTypedEventTimesAvg->Add(
          std::map<std::string, std::string>{{"step", "process"}, {"event_type", event_name}, {"event_dir", event_dir}});
    }
  }

  while (thread_.Pause(std::chrono::seconds(5))) {
    system_inspector::Stats stats;
    if (!system_inspector_->GetStats(&stats)) {
      continue;
    }

    kernel.Set(stats.nEvents);
    drops.Set(stats.nDrops);
    threadCacheDrops.Set(stats.nDropsThreadCache);
    ringbufferDrops.Set(stats.nDropsBuffer);
    preemptions.Set(stats.nPreemptions);
    threadTableSize.Set(stats.nThreadCacheSize);

    if (config_->EnableDetailedMetrics()) {
      uint64_t nUserspace = 0;
      for (int i = 0; i < PPM_EVENT_MAX; i++) {
        auto& counters = typed[i];

        auto userspace = stats.nUserspaceEvents[i];
        auto parse_micros_total = stats.event_parse_micros[i];
        auto process_micros_total = stats.event_process_micros[i];

        nUserspace += userspace;

        if (counters.userspace) counters.userspace->Set(userspace);

        if (counters.parse_micros_total) counters.parse_micros_total->Set(parse_micros_total);
        if (counters.process_micros_total) counters.process_micros_total->Set(process_micros_total);

        if (counters.parse_micros_avg) counters.parse_micros_avg->Set(userspace ? parse_micros_total / userspace : 0);
      }

      userspaceEvents->Set(nUserspace);
    }

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
