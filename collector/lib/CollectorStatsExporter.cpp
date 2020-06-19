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

#include "Containers.h"
#include "EventNames.h"
#include "SysdigService.h"
#include "CollectorStatsExporter.h"
#include "Logging.h"

#include "prometheus/registry.h"
#include "prometheus/gauge.h"

extern "C" {
    #include <pthread.h>
    #include <string.h>
}

namespace collector {

CollectorStatsExporter::CollectorStatsExporter(std::shared_ptr<prometheus::Registry> registry, const CollectorConfig* config, SysdigService* sysdig)
    : registry_(std::move(registry)), config_(config), sysdig_(sysdig)
{}

bool CollectorStatsExporter::start() {
    if (!thread_.Start(&CollectorStatsExporter::run, this)) {
        CLOG(ERROR) << "Could not start stats exporter: already running";
        return false;
    }
    return true;
}

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

    auto& collectorTypedEventCounters = prometheus::BuildGauge()
            .Name("rox_collector_events_typed")
            .Help("Collector events by event type")
            .Register(*registry_);

    struct {
        prometheus::Gauge* filtered = nullptr;
        prometheus::Gauge* userspace = nullptr;
        prometheus::Gauge* chiselCacheHitsAccept = nullptr;
        prometheus::Gauge* chiselCacheHitsReject = nullptr;
    } typed[PPM_EVENT_MAX] = {};

    const auto& active_syscalls = config_->Syscalls();
    UnorderedSet<std::string> syscall_set(active_syscalls.begin(), active_syscalls.end());

    const auto& event_names = EventNames::GetInstance();
    for (int i = 0; i < PPM_EVENT_MAX; i++) {
        auto event_name = event_names.GetEventName(i);

        if (!Contains(syscall_set, event_name)) {
            continue;
        }

        auto event_dir = event_name.substr(event_name.length() - 1);
        event_name.resize(event_name.length() - 1);

        typed[i].filtered = &collectorTypedEventCounters.Add(
                std::map<std::string, std::string>{{"quantity", "filtered"}, {"event_type", event_name}, {"event_dir", event_dir}});
        typed[i].userspace = &collectorTypedEventCounters.Add(
                std::map<std::string, std::string>{{"quantity", "userspace"}, {"event_type", event_name}, {"event_dir", event_dir}});
        typed[i].chiselCacheHitsAccept = &collectorTypedEventCounters.Add(
                std::map<std::string, std::string>{{"quantity", "chiselCacheHitsAccept"}, {"event_type", event_name}, {"event_dir", event_dir}});
        typed[i].chiselCacheHitsReject = &collectorTypedEventCounters.Add(
                std::map<std::string, std::string>{{"quantity", "chiselCacheHitsReject"}, {"event_type", event_name}, {"event_dir", event_dir}});
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

            nFiltered += filtered;
            nUserspace += userspace;
            nChiselCacheHitsAccept += chiselCacheHitsAccept;
            nChiselCacheHitsReject += chiselCacheHitsReject;

            if (counters.filtered) counters.filtered->Set(filtered);
            if (counters.userspace) counters.userspace->Set(userspace);
            if (counters.chiselCacheHitsAccept) counters.chiselCacheHitsAccept->Set(chiselCacheHitsAccept);
            if (counters.chiselCacheHitsReject) counters.chiselCacheHitsReject->Set(chiselCacheHitsReject);
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
    }
}

void CollectorStatsExporter::stop()
{
    thread_.Stop();
}


}  // namespace collector

