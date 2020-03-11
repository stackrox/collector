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

#ifndef COLLECTOR_NETWORKSTATUSNOTIFIER_H
#define COLLECTOR_NETWORKSTATUSNOTIFIER_H

#include <memory>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"

#include "ConnScraper.h"
#include "ConnTracker.h"
#include "DuplexGRPC.h"
#include "ProtoAllocator.h"
#include "StoppableThread.h"

namespace collector {

class NetworkStatusNotifier : protected ProtoAllocator<sensor::NetworkConnectionInfoMessage> {
 public:
  using Stub = sensor::NetworkConnectionInfoService::Stub;

  NetworkStatusNotifier(std::string hostname, std::string proc_dir, int scrape_interval, bool turn_off_scrape,
                        std::shared_ptr<ConnectionTracker> conn_tracker,
                        std::shared_ptr<grpc::Channel> channel)
      : hostname_(std::move(hostname)), conn_scraper_(std::move(proc_dir)), scrape_interval_(scrape_interval),
        turn_off_scraping_(turn_off_scrape),
        conn_tracker_(std::move(conn_tracker)), channel_(std::move(channel)),
        stub_(sensor::NetworkConnectionInfoService::NewStub(channel_))
  {}

  void Start();
  void Stop();

 private:
  static constexpr char kCapsMetadataKey[] = "rox-collector-capabilities";

  // Keep this updated will all capabilities supported. Format it as a comma-separated list with NO spaces.
  static constexpr char kSupportedCaps[] = "public-ips";

  sensor::NetworkConnectionInfoMessage* CreateInfoMessage(const ConnMap& conn_delta);
  sensor::NetworkConnection* ConnToProto(const Connection& conn);
  sensor::NetworkAddress* EndpointToProto(const Endpoint& endpoint);

  std::unique_ptr<grpc::ClientContext> CreateClientContext() const;

  void OnRecvControlMessage(const sensor::NetworkFlowsControlMessage* msg);

  void Run();
  void RunSingle(DuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer);

  std::string hostname_;

  StoppableThread thread_;

  std::unique_ptr<grpc::ClientContext> context_;
  std::mutex context_mutex_;

  ConnScraper conn_scraper_;
  int scrape_interval_;
  bool turn_off_scraping_;
  std::shared_ptr<ConnectionTracker> conn_tracker_;

  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<Stub> stub_;
};

}  // namespace collector

#endif //COLLECTOR_NETWORKSTATUSNOTIFIER_H
