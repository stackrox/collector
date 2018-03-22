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

#include "EventFormatter.h"

namespace collector {

EventFormatter::EventFormatter(bool is_network) {
  field_format_options_["proc.args"].replace_tabs = true;
  field_format_options_["proc.cmdline"].replace_tabs = true;
  field_format_options_["proc.exeline"].replace_tabs = true;
  field_format_options_["evt.args"].trunc_data = true;
  is_network_ = is_network;
}

bool FieldFormatter::Format(SafeBuffer* buffer, sinsp_evt* event) const {
  char* str = filter_check_->tostring(event);
  if (!str || !*str) {
    return false;
  }
  int len = strlen(str);
  if (options_.replace_tabs) {
    char *p = str;
    while ((p = static_cast<char*>(memchr(p, '\t', len - (p - str))))) {
      *p++ = '.';
    }
  }
  if (options_.trunc_data && is_network_) {
    // For network data, remove anything that comes after "data=" as this is already part of the buffer.
    char* p = static_cast<char*>(memmem(str, len, "data=", 5));
    if (p) {
      *p = '\0';
      len = p - str;
    }
  }
  int trunc_len = options_.trunc_len;
  if (trunc_len > 0 && len > trunc_len) {
    str[trunc_len] = '\0';
    len = trunc_len;
  }

  if (!buffer->AppendWhole(prefix_)) return false;
  buffer->AppendTrunc(str, len);
  return true;
}

void EventFormatter::Init(sinsp* inspector, const std::string& format_string, int field_trunc_len) {
  field_trunc_len_ = field_trunc_len;
  std::istringstream input(format_string);
  for (std::string field; std::getline(input, field, ','); ) {
    auto sep_pos = field.find(':');
    std::string prefix;
    if (sep_pos != std::string::npos) {
      prefix = field.substr(0, sep_pos + 1);
      field = field.substr(sep_pos + 1);
    }
    AddFieldFormatter(inspector, field, prefix);
  }
}

void EventFormatter::AddFieldFormatter(
    sinsp* inspector, const std::string& field, const std::string& prefix) {
  std::unique_ptr<sinsp_filter_check_iface> filter_check(sinsp_filter_check_iface::get(field, inspector));
  if (!filter_check) {
    throw std::invalid_argument("Unknown field name " + field);
  }
  FieldFormatter::FormatOptions opts;
  auto it = field_format_options_.find(field);
  if (it != field_format_options_.end()) {
    opts = it->second;
  }
  if (opts.trunc_len == 0 && field_trunc_len_ > 0) {
    opts.trunc_len = field_trunc_len_;
  }
  field_formatters_.emplace_back(field, std::move(filter_check), prefix, opts, is_network_);
}

bool EventFormatter::FormatSignal(SafeBuffer* buffer, sinsp_evt* event) {
  buffer->Append(is_network_ ? 'N' : 'S');
  for (auto& field_formatter : field_formatters_) {
    buffer->Append('\t');
    if (!field_formatter.Format(buffer, event) && field_formatter.PrefixLength() > 0) {
      buffer->Truncate(-1);
      if (buffer->remaining() < 16) break;
    }
  }
  return true;
}

}  // namespace collector
