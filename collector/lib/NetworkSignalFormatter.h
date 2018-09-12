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

#ifndef _NETWORK_SIGNAL_FORMATTER_H_
#define _NETWORK_SIGNAL_FORMATTER_H_

#include <uuid/uuid.h>

#include "ProtoSignalFormatter.h"
#include "SysdigEventExtractor.h"

#include "data/signal.pb.h"

namespace collector {

class NetworkSignalFormatter : public ProtoSignalFormatter<data::Signal> {
 public:
  using Signal = data::Signal;
  using KernelSignal = data::KernelSignal;
  using NetworkSignal = data::NetworkSignal;
  using NetworkSignalType = NetworkSignal::NetworkSignalType;
  using ConnectionInfo = data::ConnectionInfo;
  using NetworkAddress = data::NetworkAddress;
  using ProcessDetails = data::ProcessDetails;

  NetworkSignalFormatter(sinsp* inspector, const uuid_t* cluster_id, bool text_format = false)
    : ProtoSignalFormatter(text_format),
      cluster_id_(cluster_id) {
    event_extractor_.Init(inspector);
  }

 protected:
  Signal* ToProtoMessage(sinsp_evt* event) override;

 private:
  KernelSignal* CreateKernelSignal(sinsp_evt* event);
  ProcessDetails* CreateProcessDetails(sinsp_evt* event);
  NetworkSignal* CreateNetworkSignal(sinsp_evt* event, NetworkSignalType signal_type);
  ConnectionInfo* CreateConnectionInfo(sinsp_fdinfo_t* fd_info);
  NetworkAddress* CreateIPv4Address(uint32_t ip, uint16_t port);
  NetworkAddress* CreateIPv6Address(const uint32_t (&ip)[4], uint16_t port);
  NetworkAddress* CreateUnixAddress(uint64_t id, const sinsp_fdinfo_t* fd_info);

  SysdigEventExtractor event_extractor_;

  const uuid_t* cluster_id_;
};

}  // namespace collector

#endif  // _NETWORK_SIGNAL_FORMATTER_H_
