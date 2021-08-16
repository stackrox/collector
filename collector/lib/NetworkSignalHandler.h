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

#ifndef COLLECTOR_NETWORKSIGNALHANDLER_H
#define COLLECTOR_NETWORKSIGNALHANDLER_H

#include "ConnTracker.h"
#include "SignalHandler.h"
#include "SysdigEventExtractor.h"
#include "SysdigService.h"

namespace collector {

class NetworkSignalHandler final : public SignalHandler {
 public:
  explicit NetworkSignalHandler(sinsp* inspector, std::shared_ptr<ConnectionTracker> conn_tracker, SysdigStats* stats)
      : conn_tracker_(std::move(conn_tracker)), stats_(stats) {
    event_extractor_.Init(inspector);
  }

  std::string GetName() override { return "NetworkSignalHandler"; }
  Result HandleSignal(sinsp_evt* evt) override;
  std::vector<std::string> GetRelevantEvents() override;
  bool Stop() override;

 private:
  std::pair<Connection, bool> GetConnection(sinsp_evt* evt);

  SysdigEventExtractor event_extractor_;
  std::shared_ptr<ConnectionTracker> conn_tracker_;
  SysdigStats* stats_;
};

}  // namespace collector

#endif  //COLLECTOR_NETWORKSIGNALHANDLER_H
