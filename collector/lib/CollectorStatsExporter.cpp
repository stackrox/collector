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

#include <iostream>
#include <chrono>
#include <string>

#include "SysdigService.h"
#include "CollectorStatsExporter.h"
#include "Logging.h"
#include "LowLevelEventNames.h"

#include "prometheus/registry.h"

extern "C" {
    #include <pthread.h>
    #include <string.h>
}

namespace collector {

CollectorStatsExporter::CollectorStatsExporter(std::shared_ptr<prometheus::Registry> registry, SysdigService* sysdig)
    : registry_(std::move(registry)), sysdig_(sysdig)
{}

bool CollectorStatsExporter::start() {
    if (!thread_.Start(&CollectorStatsExporter::run, this)) {
        CLOG(ERROR) << "Could not start sysdig stats exporter: already running";
        return false;
    }
    return true;
}

void CollectorStatsExporter::run() {
    auto& collectorEventCounters = prometheus::BuildGauge()
        .Name("rox_collector_events")
        .Help("Collector events")
        .Register(*registry_);
    auto& kernelEventCounters = prometheus::BuildGauge()
        .Name("rox_collector_kernel_events")
        .Help("Collector kernel events")
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

    struct {
      prometheus::Gauge* total;
      prometheus::Gauge* drops_buffer;
      prometheus::Gauge* drops_pf;
      prometheus::Gauge* drops_bug;
      prometheus::Gauge* drops_voluntary;
    } per_event_type_counters[PPM_EVENT_MAX];

    const auto& low_level_event_names = LowLevelEventNames::GetInstance();
    for (int i = 0; i < PPM_EVENT_MAX; i++) {
        auto& ctrs = per_event_type_counters[i];
        const auto& event_name = low_level_event_names.GetEventName(static_cast<ppm_event_type>(i));
        ctrs.total = &kernelEventCounters.Add({{"type", "totalEvents"}, {"eventType", event_name}});
        ctrs.drops_buffer = &kernelEventCounters.Add({{"type", "bufferDrops"}, {"eventType", event_name}});
        ctrs.drops_pf = &kernelEventCounters.Add({{"type", "pageFaultDrops"}, {"eventType", event_name}});
        ctrs.drops_bug = &kernelEventCounters.Add({{"type", "bugDrops"}, {"eventType", event_name}});
        ctrs.drops_voluntary = &kernelEventCounters.Add({{"type", "voluntaryDrops"}, {"eventType", event_name}});
    }

    while (thread_.Pause(std::chrono::seconds(1))) {
        SysdigStats stats;
        scap_stats kernel_stats;
        if (!sysdig_->GetStats(&stats, &kernel_stats)) {
            continue;
        }

        kernel.Set(stats.nEvents);
        drops.Set(stats.nDrops);
        preemptions.Set(stats.nPreemptions);
        filtered.Set(stats.nFilteredEvents);
        userspaceEvents.Set(stats.nUserspaceEvents);
        chiselCacheHitsAccept.Set(stats.nChiselCacheHitsAccept);
        chiselCacheHitsReject.Set(stats.nChiselCacheHitsReject);
        grpcSendFailures.Set(stats.nGRPCSendFailures);

        // process related metrics
        processSent.Set(stats.nProcessSent);
        processSendFailures.Set(stats.nProcessSendFailures);
        processResolutionFailuresByEvt.Set(stats.nProcessResolutionFailuresByEvt);
        processResolutionFailuresByTinfo.Set(stats.nProcessResolutionFailuresByTinfo);
        processRateLimitCount.Set(stats.nProcessRateLimitCount);

        for (int i = 0; i < PPM_EVENT_MAX; i++) {
            auto& ctrs = per_event_type_counters[i];
            ctrs.total->Set(kernel_stats.n_evts_per_type[i]);
            ctrs.drops_buffer->Set(kernel_stats.n_drops_buffer_per_event[i]);
            ctrs.drops_pf->Set(kernel_stats.n_drops_pf_per_event[i]);
            ctrs.drops_bug->Set(kernel_stats.n_drops_bug_per_event[i]);
            ctrs.drops_voluntary->Set(kernel_stats.n_drops_voluntary_per_event[i]);
        }
    }
}

void CollectorStatsExporter::stop()
{
    thread_.Stop();
}


}  // namespace collector

