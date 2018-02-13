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

#ifndef _EVENT_FORMATTER_H_
#define _EVENT_FORMATTER_H_

#include <memory>
#include <string>
#include <vector>

#include "libsinsp/sinsp.h"

#include "EventClassifier.h"
#include "SafeBuffer.h"

namespace collector {

class FieldFormatter {
 public:
  struct FormatOptions {
    int trunc_len = 0;
    bool trunc_data = false;
    bool replace_tabs = false;
  };

  FieldFormatter(std::string field, std::unique_ptr<sinsp_filter_check_iface> filter_check, std::string prefix, const FormatOptions& options)
      : field_(std::move(field)), filter_check_(std::move(filter_check)), prefix_(std::move(prefix)), options_(options) {}

  bool Format(SafeBuffer* buffer, sinsp_evt* event, SignalType signal_type) const;

  int PrefixLength() const { return prefix_.length(); }

 private:
  std::string field_;
  std::unique_ptr<sinsp_filter_check_iface> filter_check_;
  std::string prefix_;
  FormatOptions options_;
};

class EventFormatter {
 public:
  EventFormatter();

  void Init(sinsp* inspector, const std::string& format_string, int field_trunc_len = 0);

  void Format(SafeBuffer* buffer, sinsp_evt* event, SignalType signal_type) const;

 private:
  void AddFieldFormatter(sinsp* inspector, const std::string& field, const std::string& prefix);

  std::unordered_map<std::string, FieldFormatter::FormatOptions> field_format_options_;
  std::vector<FieldFormatter> field_formatters_;
  int field_trunc_len_;
};

}  // namespace collector

#endif  // _EVENT_FORMATTER_H_
