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

#include <atomic>
#include <string>

extern "C" {
#include <stdint.h>
}

namespace collector {

struct SysdigStats {
  // stats gathered in kernel space
  volatile uint64_t nEvents = 0;      // the number of kernel events
  volatile uint64_t nDrops = 0;       // the number of drops
  volatile uint64_t nPreemptions = 0; // the number of preemptions

  // stats gathered in user space
  volatile uint64_t nFilteredEvents = 0;    // events post chisel filter
  volatile uint64_t nUserspaceEvents = 0;   // events pre chisel filter, should be (nEvents - nDrops)
  volatile uint64_t nKafkaSendFailures = 0; // number of signals that were not sent
};

class Sysdig {
 public:
  virtual ~Sysdig() = default;

  virtual void Init(const std::string& chiselName, const std::string& brokerList, const std::string& format,
                    const std::string& networkTopic, const std::string& processTopic, const std::string& fileTopic,
                    const std::string& processSyscalls, int snaplen) = 0;
  virtual void RunForever(const std::atomic_bool& interrupt) = 0;
  virtual void CleanUp() = 0;

  virtual bool GetStats(SysdigStats* stats) const = 0;
};

}   /* namespace collector */

#endif  /* _SYSDIG_ */
