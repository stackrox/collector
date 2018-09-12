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

#ifndef _FILE_SUMMARY_FORMATTER_H_
#define _FILE_SUMMARY_FORMATTER_H_

#include "ProtoSignalFormatter.h"
#include "EventNames.h"

#include "data/file_summary.pb.h"

namespace collector {

class FileSummaryFormatter : public ProtoSignalFormatter<data::FileSummary> {
 public:
  FileSummaryFormatter(sinsp* inspector, const uuid_t* cluster_id, bool text_format = false)
      : ProtoSignalFormatter(text_format), event_names_(EventNames::GetInstance()),
        cluster_id_(cluster_id) {
    event_extractor_.Init(inspector);
  }

  using FileContainer = data::FileSummary::Container;
  using FileDetails = data::FileSummary::File;
  using FileOp = data::FileSummary::FileOp;
  using FileOperation = data::FileSummary::FileOperation;
  using FileProcess = data::FileSummary::Process;
  using FileSummary = data::FileSummary;

 protected:
  const data::FileSummary* ToProtoMessage(sinsp_evt* event);

 private:
  FileOperation* CreateFileOperation(sinsp_evt* event, FileOp op);
  FileContainer* CreateFileContainer(sinsp_evt* event);
  FileProcess* CreateFileProcess(sinsp_evt* event);
  FileDetails* CreateFileDetails(sinsp_evt* event);
  void AddFileOperations(sinsp_evt* event, FileSummary* file_summary, FileOp op);
  std::string GetPathAtFD(sinsp_evt* event, const int64_t* fd, const char* path);

  const EventNames& event_names_;
  SysdigEventExtractor event_extractor_;
  const uuid_t* cluster_id_;
};

}  // namespace collector

#endif  // _FILE_SUMMARY_FORMATTER_H_
