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

extern "C" {
#include <uuid/uuid.h>
}

#include "SignalFormatter.h"

#include "CollectorException.h"
#include "EventFormatter.h"
#include "FileSummaryFormatter.h"
#include "NetworkSignalFormatter.h"
#include "ProcessSummaryFormatter.h"
#include "Utility.h"

namespace collector {

SignalFormatterFactory::SignalFormatterFactory(sinsp* inspector, const uuid_t* cluster_id)
  : cluster_id_(cluster_id)
{
  extractor_.Init(inspector);
} 

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateSignalFormatter(const std::string& format_type, sinsp* inspector, const std::string& format_string, int field_trunc_len) {
  if (format_type == "file_summary") {
    return CreateFileSummaryFormatter(inspector, false);
  }
  if (format_type == "file_summary_text") {
    return CreateFileSummaryFormatter(inspector, true);
  }
  if (format_type == "process_summary") {
    return CreateProcessSummaryFormatter(inspector, false);
  }
  if (format_type == "process_summary_text") {
    return CreateProcessSummaryFormatter(inspector, true);
  }
  if (format_type == "network_signal") {
    return MakeUnique<NetworkSignalFormatter>(inspector, cluster_id_, false);
  }
  if (format_type == "network_signal_text") {
      return MakeUnique<NetworkSignalFormatter>(inspector, cluster_id_, true);
  }
  if (format_type == "process_legacy") {
    return CreateProcessLegacyFormatter(inspector, format_string, field_trunc_len);
  }
  throw CollectorException("Invalid format type '" + format_type);
}

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateFileSummaryFormatter(sinsp* inspector, bool text_format) {
  return MakeUnique<FileSummaryFormatter>(inspector, text_format);
}

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateProcessSummaryFormatter(sinsp* inspector, bool text_format) {
  return MakeUnique<ProcessSummaryFormatter>(inspector, text_format);
}

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateProcessLegacyFormatter(sinsp* inspector, const std::string& format_string, int field_trunc_len) {
  EventFormatter *f = new EventFormatter(false);
  f->Init(inspector, format_string, field_trunc_len);
  return std::unique_ptr<SignalFormatter>(f);
}

}  // namespace collector
