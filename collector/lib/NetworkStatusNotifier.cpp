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

#include "NetworkStatusNotifier.h"

#include "Containers.h"
#include "DuplexGRPC.h"
#include "GRPCUtil.h"
#include "ProtoUtil.h"
#include "TimeUtil.h"
#include "Utility.h"

#include <google/protobuf/util/time_util.h>

namespace collector {

namespace {

storage::L4Protocol TranslateL4Protocol(L4Proto proto) {
  switch (proto) {
    case L4Proto::TCP:
      return storage::L4_PROTOCOL_TCP;
    case L4Proto::UDP:
      return storage::L4_PROTOCOL_UDP;
    case L4Proto::ICMP:
      return storage::L4_PROTOCOL_ICMP;
    default:
      return storage::L4_PROTOCOL_UNKNOWN;
  }
}

sensor::SocketFamily TranslateAddressFamily(Address::Family family) {
  switch (family) {
    case Address::Family::IPV4:
      return sensor::SOCKET_FAMILY_IPV4;
    case Address::Family::IPV6:
      return sensor::SOCKET_FAMILY_IPV6;
    default:
      return sensor::SOCKET_FAMILY_UNKNOWN;
  }
}

}  // namespace

std::unique_ptr<grpc::ClientContext> NetworkStatusNotifier::CreateClientContext() const {
  auto ctx = MakeUnique<grpc::ClientContext>();
  ctx->AddMetadata("rox-collector-hostname", hostname_);
  return ctx;
}

void NetworkStatusNotifier::Run() {
  auto next_attempt = std::chrono::system_clock::now();

  while (thread_.PauseUntil(next_attempt)) {
    WITH_LOCK(context_mutex_) {
      context_ = CreateClientContext();
    }

    if (!WaitForChannelReady(channel_, [this] { return thread_.should_stop(); })) {
      break;
    }

    auto client_writer = DuplexClient::CreateWithReadsIgnored(
        &sensor::NetworkConnectionInfoService::Stub::AsyncPushNetworkConnectionInfo,
        channel_, context_.get());

    RunSingle(client_writer.get());
    if (thread_.should_stop()) {
      return;
    }
    auto status = client_writer->Finish(std::chrono::seconds(5));
    if (status.ok()) {
      CLOG(ERROR) << "Error streaming network connection info: server hung up unexpectedly";
    } else {
      CLOG(ERROR) << "Error streaming network connection info: " << status.error_message();
    }
    next_attempt = std::chrono::system_clock::now() + std::chrono::seconds(10);
  }

  CLOG(INFO) << "Stopped network status notifier.";
}

void NetworkStatusNotifier::Start() {
  thread_.Start([this]{ Run(); });
  CLOG(INFO) << "Started network status notifier.";
}

void NetworkStatusNotifier::Stop() {
  WITH_LOCK(context_mutex_) {
    if (context_) context_->TryCancel();
  }
  thread_.Stop();
}

void NetworkStatusNotifier::RunSingle(DuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer) {
  if (!writer->WaitUntilStarted(std::chrono::seconds(10))) {
    CLOG(ERROR) << "Failed to establish network connection info stream.";
    return;
  }

  ConnMap old_state;
  auto next_scrape = std::chrono::system_clock::now();

  while (writer->Sleep(next_scrape)) {
    next_scrape = std::chrono::system_clock::now() + std::chrono::seconds(scrape_interval_);

    if (!turn_off_scraping_) {
      int64_t ts = NowMicros();
      std::vector<Connection> all_conns;
      bool success = conn_scraper_.Scrape(&all_conns);

      if (!success) {
        CLOG(ERROR) << "Failed to scrape connections and no pending connections to send";
        continue;
      }
      conn_tracker_->Update(all_conns, ts);
    }

    auto new_state = conn_tracker_->FetchState(true, true);
    ConnectionTracker::ComputeDelta(new_state, &old_state);

    const auto* msg = CreateInfoMessage(old_state);
    old_state = std::move(new_state);

    if (!msg) {
      continue;
    }

    if (!writer->Write(*msg, next_scrape)) {
      CLOG(ERROR) << "Failed to write network connection info";
      return;
    }
  }
}

sensor::NetworkConnectionInfoMessage* NetworkStatusNotifier::CreateInfoMessage(const ConnMap& delta) {
  if (delta.empty()) {
    return nullptr;
  }

  Reset();
  auto* msg = AllocateRoot();

  auto* info_msg = Allocate<sensor::NetworkConnectionInfo>();
  *info_msg->mutable_time() = CurrentTimeProto();
  auto* updates = info_msg->mutable_updated_connections();

  for (const auto& delta_entry : delta) {
    auto* conn_proto = ConnToProto(delta_entry.first);
    if (!delta_entry.second.IsActive()) {
      *conn_proto->mutable_close_timestamp() = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
          delta_entry.second.LastActiveTime());
    }
    updates->AddAllocated(conn_proto);
  }

  msg->set_allocated_info(info_msg);
  return msg;
}

sensor::NetworkConnection* NetworkStatusNotifier::ConnToProto(const Connection& conn) {
  auto* conn_proto = Allocate<sensor::NetworkConnection>();
  conn_proto->set_container_id(conn.container());
  conn_proto->set_role(conn.is_server() ? sensor::ROLE_SERVER : sensor::ROLE_CLIENT);
  conn_proto->set_protocol(TranslateL4Protocol(conn.l4proto()));
  conn_proto->set_socket_family(TranslateAddressFamily(conn.local().address().family()));
  conn_proto->set_allocated_local_address(EndpointToProto(conn.local()));
  conn_proto->set_allocated_remote_address(EndpointToProto(conn.remote()));

  return conn_proto;
}

sensor::NetworkAddress* NetworkStatusNotifier::EndpointToProto(const collector::Endpoint& endpoint) {
  if (endpoint.IsNull()) {
    return nullptr;
  }

  auto* addr_proto = Allocate<sensor::NetworkAddress>();
  if (!endpoint.address().IsNull()) {
    addr_proto->set_address_data(endpoint.address().data(), endpoint.address().length());
  }
  addr_proto->set_port(endpoint.port());

  return addr_proto;
}

}  // namespace collector
