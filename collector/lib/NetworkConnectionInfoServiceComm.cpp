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

#include "NetworkConnectionInfoServiceComm.h"

#include "GRPCUtil.h"
#include "Utility.h"

namespace collector {

constexpr char NetworkConnectionInfoServiceComm::kHostnameMetadataKey[];
constexpr char NetworkConnectionInfoServiceComm::kCapsMetadataKey[];
constexpr char NetworkConnectionInfoServiceComm::kSupportedCaps[];

std::unique_ptr<grpc::ClientContext> NetworkConnectionInfoServiceComm::CreateClientContext() const {
  auto ctx = MakeUnique<grpc::ClientContext>();
  ctx->AddMetadata(kHostnameMetadataKey, hostname_);
  ctx->AddMetadata(kCapsMetadataKey, kSupportedCaps);
  return ctx;
}

NetworkConnectionInfoServiceComm::NetworkConnectionInfoServiceComm(std::string hostname, std::shared_ptr<grpc::Channel> channel) : hostname_(std::move(hostname)), channel_(std::move(channel)), stub_(sensor::NetworkConnectionInfoService::NewStub(channel_)) {
}

void NetworkConnectionInfoServiceComm::ResetClientContext() {
  WITH_LOCK(context_mutex_) {
    context_ = CreateClientContext();
  }
}

bool NetworkConnectionInfoServiceComm::WaitForConnectionReady(const std::function<bool()>& check_interrupted) {
  return WaitForChannelReady(channel_, check_interrupted);
}

void NetworkConnectionInfoServiceComm::TryCancel() {
  WITH_LOCK(context_mutex_) {
    if (context_) context_->TryCancel();
  }
}

std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> NetworkConnectionInfoServiceComm::PushNetworkConnectionInfoOpenStream(std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) {
  if (!context_)
    ResetClientContext();

  return DuplexClient::CreateWithReadCallback(
      &sensor::NetworkConnectionInfoService::Stub::AsyncPushNetworkConnectionInfo,
      channel_, context_.get(), std::move(receive_func));
}

}  // namespace collector
