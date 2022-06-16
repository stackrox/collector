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

#ifndef COLLECTOR_NETWORKCONNECTIONINFOSERVICECOMM_H
#define COLLECTOR_NETWORKCONNECTIONINFOSERVICECOMM_H

#include <memory>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"

#include "DuplexGRPC.h"

namespace collector {

// Gathers all the communication routines targeted at NetworkConnectionInfoService.
// A simple gRPC mock is not sufficient for testing, since it doesn't abstract Streams.
class INetworkConnectionInfoServiceComm {
 public:
  virtual ~INetworkConnectionInfoServiceComm() {}

  virtual void ResetClientContext() = 0;
  // return false on failure
  virtual bool WaitForConnectionReady(const std::function<bool()>& check_interrupted) = 0;
  virtual void TryCancel() = 0;

  virtual sensor::NetworkConnectionInfoService::StubInterface* GetStub() = 0;

  virtual std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> PushNetworkConnectionInfoOpenStream(std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) = 0;
};

class NetworkConnectionInfoServiceComm : public INetworkConnectionInfoServiceComm {
 public:
  NetworkConnectionInfoServiceComm(std::string hostname, std::shared_ptr<grpc::Channel> channel);

  void ResetClientContext() override;
  bool WaitForConnectionReady(const std::function<bool()>& check_interrupted) override;
  void TryCancel() override;

  sensor::NetworkConnectionInfoService::StubInterface* GetStub() override {
    return stub_.get();
  }

  std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> PushNetworkConnectionInfoOpenStream(std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) override;

 private:
  static constexpr char kHostnameMetadataKey[] = "rox-collector-hostname";
  static constexpr char kCapsMetadataKey[] = "rox-collector-capabilities";

  // Keep this updated with all capabilities supported. Format it as a comma-separated list with NO spaces.
  static constexpr char kSupportedCaps[] = "public-ips,network-graph-external-srcs";

  std::unique_ptr<grpc::ClientContext> CreateClientContext() const;

  std::string hostname_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<sensor::NetworkConnectionInfoService::Stub> stub_;

  std::mutex context_mutex_;
  std::unique_ptr<grpc::ClientContext> context_;
};

}  // namespace collector

#endif
