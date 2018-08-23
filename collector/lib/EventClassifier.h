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

#ifndef _EVENT_CLASSIFIER_H_
#define _EVENT_CLASSIFIER_H_

#include <bitset>

#include "libsinsp/sinsp.h"
#include "ppm_events_public.h"

#include "SafeBuffer.h"

namespace collector {

enum SignalType {
    SIGNAL_TYPE_UNKNOWN = 0,
    SIGNAL_TYPE_NETWORK,
    SIGNAL_TYPE_PROCESS,
    SIGNAL_TYPE_FILE,
    SIGNAL_TYPE_GENERIC,
    SIGNAL_TYPE_MAX = SIGNAL_TYPE_GENERIC
};

class EventClassifier {
 public:
  void Init(const std::string& hostname, const std::vector<std::string>& process_syscalls, const std::vector<std::string>& generic_syscalls);

  SignalType Classify(SafeBuffer* key_buf, sinsp_evt* event) const;

 private:
  static bool ExtractProcessSignalKey(SafeBuffer* key_buf, sinsp_evt* event);
  bool ExtractNetworkSignalKey(SafeBuffer* key_buf, sinsp_evt* event) const;
  static bool ExtractFileSignalKey(SafeBuffer* key_buf, sinsp_evt* event);

  uint64_t hostname_hash_;
  std::bitset<PPM_EVENT_MAX> process_syscalls_;
  std::bitset<PPM_EVENT_MAX> generic_syscalls_;
};

}  // namespace collector

#endif  // _EVENT_CLASSIFIER_H_
