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

#ifndef _SYSDIG_
#define _SYSDIG_

#include <string>

#include "KafkaClient.h"

extern "C" {
    #include <sys/types.h>
    #include <inttypes.h>
}

namespace collector {

struct SysdigStats {
  uint64_t    nEvents;                // the number of kernel events
  uint64_t    nDrops;                 // the number of drops
  uint64_t    nPreemptions;           // the number of preemptions
  uint32_t    mLinePeriodicity;       // periodicity of M lines
  uint64_t    nEventsDelta;           // events since last M line
  uint64_t    nDropsDelta;            // drops since last M line
  uint64_t    nPreemptionsDelta;      // preemptions since last M line
  uint32_t    nUpdates;               // number of topic map updates
  uint64_t    nFilteredEvents;        // events post chisel filter
  std::string nodeName;               // the name of this node (hostname)
};

class Sysdig {
    public:
    virtual ~Sysdig() {};

    virtual int init(std::string chiselName, std::string brokerList, std::string format,
                     bool useKafka, std::string defaultTopic, std::string networkTopic,
                     int snapLen) = 0;
    virtual bool ready() = 0;
    virtual void runForever() = 0;
    virtual void cleanup() = 0;

    virtual bool stats(SysdigStats &s) = 0;
    virtual KafkaClient *getKafkaClient() = 0;
};

}   /* namespace collector */

#endif  /* _SYSDIG_ */

