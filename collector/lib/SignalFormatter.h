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

#ifndef _SUMMARY_FORMATTER_FACTORY_H_
#define _SUMMARY_FORMATTER_FACTORY_H_

extern "C" {
#include <uuid/uuid.h>
}

#include "SafeBuffer.h"
#include "SysdigEventExtractor.h"

namespace collector {

class SignalFormatter {
 public:
  virtual bool FormatSignal(SafeBuffer* buf, sinsp_evt* event) = 0;
  virtual bool FormatSignal(SafeBuffer* buf, sinsp_threadinfo* tinfo) = 0;
  virtual ~SignalFormatter() = default;
};

class SignalFormatterFactory {
 public:
  SignalFormatterFactory(sinsp* inspector, const uuid_t* cluster_id);
  std::unique_ptr<SignalFormatter> CreateSignalFormatter(const std::string& output_spec, sinsp* inspector);

 private:
  // methods to convert from sysdig event to summaries
  std::unique_ptr<SignalFormatter> CreateFileSummaryFormatter(sinsp* inspector, bool text_format = false);
  std::unique_ptr<SignalFormatter> CreateProcessSummaryFormatter(sinsp* inspector, bool text_format = false);

  SysdigEventExtractor extractor_;
  const uuid_t* cluster_id_;
};

}  // namespace collector

#endif  // _SUMMARY_FORMATTER_FACTORY_H_
