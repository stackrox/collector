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

#ifndef _COLLECTOR_SIGNAL_FORMATTER_H_
#define _COLLECTOR_SIGNAL_FORMATTER_H_

#include "EventNames.h"
#include "ProtoSignalFormatter.h"
#include "SysdigEventExtractor.h"

#include "../generated/proto/api/v1/signal.pb.h"

namespace collector {

class CollectorSignalFormatter : public ProtoSignalFormatter<v1::SignalStreamMessage> {
 public:
  CollectorSignalFormatter(sinsp* inspector, const uuid_t* cluster_id, bool text_format = false)
      : ProtoSignalFormatter(text_format), event_names_(EventNames::GetInstance()), 
          cluster_id_(cluster_id) {
    event_extractor_.Init(inspector);
  }

  using Signal = v1::Signal;
  using ProcessSignal = v1::ProcessSignal;
  using ProcessCredentials = v1::ProcessSignal_Credentials;

  using NetworkSignal = v1::NetworkSignal;
  using NetworkAddress = v1::NetworkAddress;
  using L4Protocol = v1::L4Protocol;
  using IPV4NetworkAddress = v1::IPV4NetworkAddress;
  using IPV6NetworkAddress = v1::IPV6NetworkAddress;

 protected:
  const v1::SignalStreamMessage* ToProtoMessage(sinsp_evt* event) override;
  const v1::SignalStreamMessage* ToProtoMessage(sinsp_threadinfo* tinfo) override;

 private:
  Signal* CreateSignal(sinsp_evt* event);
  ProcessSignal* CreateProcessSignal(sinsp_evt* event);
  ProcessCredentials* CreateProcessCreds(sinsp_evt* event);

  NetworkSignal* CreateNetworkSignal(sinsp_evt* event);
  NetworkAddress* CreateIPv4Address(uint32_t ip, uint16_t port);
  NetworkAddress* CreateIPv6Address(const uint32_t (&ip)[4], uint16_t port);
  NetworkAddress* CreateUnixAddress(uint64_t id, const sinsp_fdinfo_t* fd_info);


  Signal* CreateSignal(sinsp_threadinfo* tinfo);
  ProcessSignal* CreateProcessSignal(sinsp_threadinfo* tinfo);
  ProcessCredentials* CreateProcessCreds(sinsp_threadinfo* tinfo);

  const EventNames& event_names_;
  SysdigEventExtractor event_extractor_;
  const uuid_t* cluster_id_;
};

}  // namespace collector

#endif  // _COLLECTOR_SIGNAL_FORMATTER_H_
