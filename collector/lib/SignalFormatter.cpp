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
#include "SignalFormatter.h"

#include "CollectorException.h"
#include "FileSummaryFormatter.h"
#include "ProcessSummaryFormatter.h"
#include "EventFormatter.h"

namespace collector {

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateSignalFormatter(const std::string& format_type, sinsp* inspector, const std::string& format_string, int field_trunc_len) {

  if (format_type == "file_summary") {
    return CreateFileSummaryFormatter();
  }
  if (format_type == "process_summary") {
    return CreateProcessSummaryFormatter();
  }
//  if (format_type == "network_summary") {
//    return CreateNetworkSummaryFormatter();
//  }
//  if (format_type == "file_proto") {
//    return CreateFileSignalFormatter();
//  }
//  if (format_type == "process_proto") {
//    return CreateProcessSignalFormatter();
//  }
//  if (format_type == "network_proto") {
//    return CreateNetworkSignalFormatter();
//  }
  if (format_type == "file_legacy") {
    return CreateFileLegacyFormatter(inspector, format_string, field_trunc_len);
  }
  if (format_type == "process_legacy") {
    return CreateProcessLegacyFormatter(inspector, format_string, field_trunc_len);
  }
  if (format_type == "network_legacy") {
    return CreateNetworkLegacyFormatter(inspector, format_string, field_trunc_len);
  }
  throw CollectorException("Invalid format type '" + format_type);
}

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateFileSummaryFormatter() {
  return std::unique_ptr<SignalFormatter>(new FileSummaryFormatter());
}

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateProcessSummaryFormatter() {
  return std::unique_ptr<SignalFormatter>(new ProcessSummaryFormatter());
}
//
//std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateNetworkSummaryFormatter() {
//  return std::unique_ptr<SignalFormatter>(new NetworkSummaryFormatter());
//}
//
//std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateFileSignalFormatter() {
//  return std::unique_ptr<SignalFormatter>(new FileSignalFormatter());
//}
//
//std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateProcessSignalFormatter() {
//  return std::unique_ptr<SignalFormatter>(new ProcessSignalFormatter());
//}
//
//std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateNetworkSignalFormatter() {
//  return std::unique_ptr<SignalFormatter>(new NetworkSignalFormatter());
//}

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateFileLegacyFormatter(sinsp* inspector, const std::string& format_string, int field_trunc_len)  {
  EventFormatter *f = new EventFormatter(false);
  f->Init(inspector, format_string, field_trunc_len);
  return std::unique_ptr<SignalFormatter>(f);
}

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateProcessLegacyFormatter(sinsp* inspector, const std::string& format_string, int field_trunc_len) {
  EventFormatter *f = new EventFormatter(false);
  f->Init(inspector, format_string, field_trunc_len);
  return std::unique_ptr<SignalFormatter>(f);
}

std::unique_ptr<SignalFormatter> SignalFormatterFactory::CreateNetworkLegacyFormatter(sinsp* inspector, const std::string& format_string, int field_trunc_len) {
  EventFormatter *f = new EventFormatter(true);
  f->Init(inspector, format_string, field_trunc_len);
  return std::unique_ptr<SignalFormatter>(f);
}

}  // namespace collector
