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

#ifndef _EVENT_NAMES_H_
#define _EVENT_NAMES_H_

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "ppm_events_public.h"

#include "CollectorException.h"
#include "Utility.h"

namespace collector {

class EventNames {
 public:
  using EventIDVector = std::vector<ppm_event_type>;

  static const EventNames& GetInstance();

  const EventIDVector& GetEventIDs(const std::string& name) const {
    auto it = events_by_name_.find(name);
    if (it == events_by_name_.end()) {
      throw CollectorException("Invalid event name '" + name + "'");
    }
    return it->second;
  }

  // Return event name for given event id
  const std::string& GetEventName(uint16_t id) const {
    if (id < 0 || id >= names_by_id_.size()) {
      throw CollectorException(Str("Invalid event id ", id));
    }
    return names_by_id_[id];
  }

  // Return associated syscall id for given event id
  uint16_t GetEventSyscallID(uint16_t id) const {
    if (id < 0 || id >= syscall_by_id_.size()) {
      throw CollectorException(Str("Invalid event id ", id));
    }
    return syscall_by_id_[id];
  }

 private:
  EventNames();

  std::unordered_map<std::string, EventIDVector> events_by_name_;
  std::array<std::string, PPM_EVENT_MAX> names_by_id_;
  std::array<uint16_t, PPM_EVENT_MAX> syscall_by_id_;
};

}  // namespace collector

#endif  // _EVENT_NAMES_H_
