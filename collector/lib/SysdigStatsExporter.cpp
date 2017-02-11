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
#include <thread>

#include "SysdigService.h"
#include "SysdigStatsExporter.h"

#include "prometheus/registry.h"

extern "C" {
    #include <pthread.h>
    #include <string.h>
}

namespace collector {

void * callback(void * arg)
{
    SysdigStatsExporter *exporter = (SysdigStatsExporter *) arg;
    exporter->run();
    return NULL;
}

SysdigStatsExporter::SysdigStatsExporter(std::shared_ptr<prometheus::Registry> aRegistry, SysdigService *theSysdig)
    : registry(aRegistry), sysdig(theSysdig)
{
}

SysdigStatsExporter::~SysdigStatsExporter(){
}

bool
SysdigStatsExporter::start()
{
    int rv = pthread_create(&this->thread, NULL, callback, this);
    if (rv != 0) {
        std::cerr << "Error creating sysdig stats exporter: " << strerror(rv) << std::endl;
        return false;
    }
    return true;
}

void
SysdigStatsExporter::run()
{
    auto& sysdigCounters = prometheus::BuildGauge()
        .Name("sysdig_events")
        .Help("Sysdig events")
        .Register(*this->registry);

    auto& kernel = sysdigCounters.Add({{"type", "kernel"}});
    auto& drops = sysdigCounters.Add({{"type", "drops"}});
    auto& preemptions = sysdigCounters.Add({{"type", "preemptions"}});
    auto& filtered = sysdigCounters.Add({{"type", "filtered"}});

    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (!sysdig->ready()) {
            continue;
        }

        SysdigStats stats;
        if (!sysdig->stats(stats)) {
            continue;
        }

        kernel.Set(stats.nEvents);
        drops.Set(stats.nDrops);
        preemptions.Set(stats.nPreemptions);
        filtered.Set(stats.nFilteredEvents);
    }
}

}   /* namespace collector */

