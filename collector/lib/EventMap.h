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

#ifndef _EVENT_MAP_H_
#define _EVENT_MAP_H_

#include <array>
#include <string>
#include <utility>

#include "ppm_events_public.h"

#include "EventNames.h"
#include "Utility.h"

namespace collector {

template <typename T>
class EventMap {
 public:
  EventMap() : EventMap(T()) {}
  EventMap(const T& initVal) : event_names_(EventNames::GetInstance()) {
    values_.fill(initVal);
  }
  EventMap(std::initializer_list<std::pair<std::string, T>> init, const T& initVal = T()) : EventMap(initVal) {
    for (const auto& pair : init) {
      Set(pair.first, pair.second);
    }
  }

  T& operator[](uint16_t id) {
    if (id < 0 || id >= values_.size()) {
      throw CollectorException(Str("Invalid event id ", id));
    }
    return values_[id];
  }

  const T& operator[](uint16_t id) const {
    if (id < 0 || id >= values_.size()) {
      throw CollectorException(Str("Invalid event id ", id));
    }
    return values_[id];
  }

  void Set(const std::string& name, const T& value) {
    for (auto event_id : event_names_.GetEventIDs(name)) {
      values_[event_id] = value;
    }
  }

 private:
  const EventNames& event_names_;
  std::array<T, PPM_EVENT_MAX> values_;
};

}  // namespace collector

#endif  // _EVENT_MAP_H_
