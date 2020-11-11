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

#include "EventNames.h"

extern const struct ppm_event_info g_event_info[];  // defined in libscap

namespace collector {

const EventNames& EventNames::GetInstance() {
  static EventNames* event_names = new EventNames;
  return *event_names;
}

EventNames::EventNames() {
  for (int i = 0; i < PPM_EVENT_MAX; i++) {
    std::string name(g_event_info[i].name);
    names_by_id_[i] = name;
    ppm_event_type event_type(static_cast<ppm_event_type>(i));
    events_by_name_[name].push_back(event_type);
    if (PPME_IS_ENTER(event_type)) {
      events_by_name_[name + ">"].push_back(event_type);
    } else {
      events_by_name_[name + "<"].push_back(event_type);
    }
  }
}

}  // namespace collector
