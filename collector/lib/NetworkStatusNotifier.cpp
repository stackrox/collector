//
// Created by Malte Isberner on 9/18/18.
//

#include "NetworkStatusNotifier.h"

#include "GRPCUtil.h"
#include "TimeUtil.h"
#include "Utility.h"

#include <google/protobuf/util/time_util.h>

namespace collector {

namespace {

sensor::L4Protocol TranslateL4Protocol(L4Proto proto) {
  switch (proto) {
    case L4Proto::TCP:
      return sensor::L4_PROTOCOL_TCP;
    case L4Proto::UDP:
      return sensor::L4_PROTOCOL_UDP;
    case L4Proto::ICMP:
      return sensor::L4_PROTOCOL_ICMP;
    default:
      return sensor::L4_PROTOCOL_UNKNOWN;
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

void NetworkStatusNotifier::Run() {
  sensor::NetworkConnectionInfoMessage initial_message;
  initial_message.mutable_register_()->set_hostname(hostname_);

  CLOG(INFO) << "Starting ...";

  while (!thread_.should_stop()) {
    WITH_LOCK(context_mutex_) {
      context_ = MakeUnique<grpc::ClientContext>();
    }
    v1::Empty empty;

    CLOG(INFO) << "Waiting for channel to become ready";
    if (!WaitForChannelReady(channel_, [this] { return thread_.should_stop(); })) {
      break;
    }

    CLOG(INFO) << "Channel is ready";

    auto client_writer = stub_->PushNetworkConnectionInfo(context_.get(), &empty);

    CLOG(INFO) << "Trying to write";
    if (!client_writer->Write(initial_message)) {
      auto status = client_writer->Finish();
      CLOG(ERROR) << "Failed to send collector registration request: " << status.error_message();
      CLOG(ERROR) << "Sleeping for 5 seconds";
      thread_.Pause(std::chrono::seconds(5));  // no need to check return value as loop condition does that.
      continue;
    }

    CLOG(INFO) << "Successfully wrote etc pp";

    if (!RunSingle(client_writer.get())) {
      break;
    }
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

bool NetworkStatusNotifier::RunSingle(grpc::ClientWriter<sensor::NetworkConnectionInfoMessage>* writer) {
  ConnMap old_state;

  do {
    int64_t ts = NowMicros();
    std::vector<Connection> all_conns;
    if (conn_scraper_.Scrape(&all_conns)) {
      conn_tracker_->Update(all_conns, ts);
    } else {
      CLOG(ERROR) << "Failed to scrape connections";
    }

    auto new_state = conn_tracker_->FetchState(true);
    ConnectionTracker::ComputeDelta(new_state, &old_state);

    const auto* msg = CreateInfoMessage(old_state);
    old_state = std::move(new_state);

    if (!msg) continue;

    if (!writer->Write(*msg)) {
      break;
    }
  } while (thread_.Pause(std::chrono::seconds(30)));

  auto status = writer->Finish();
  if (!status.ok()) {
    CLOG(ERROR) << "Failed to write network connection info: " << status.error_message();
  }

  return !thread_.should_stop();
}

sensor::NetworkConnectionInfoMessage* NetworkStatusNotifier::CreateInfoMessage(const ConnMap& delta) {
  if (delta.empty()) {
    return nullptr;
  }

  Reset();
  auto* msg = AllocateRoot();

  auto* info_msg = Allocate<sensor::NetworkConnectionInfo>();
  *info_msg->mutable_time() = google::protobuf::util::TimeUtil::GetCurrentTime();
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
  conn_proto->set_role(conn.is_server() ? sensor::NetworkConnection::ROLE_SERVER : sensor ::NetworkConnection::ROLE_CLIENT);
  conn_proto->set_protocol(TranslateL4Protocol(conn.l4proto()));
  conn_proto->set_socket_family(TranslateAddressFamily(conn.local().address().family()));
  conn_proto->set_allocated_local_address(EndpointToProto(conn.local()));
  conn_proto->set_allocated_remote_address(EndpointToProto(conn.remote()));

  return conn_proto;
}

sensor::NetworkAddress* NetworkStatusNotifier::EndpointToProto(const collector::Endpoint& endpoint) {
  auto* addr_proto = Allocate<sensor::NetworkAddress>();
  addr_proto->set_address_data(endpoint.address().data(), endpoint.address().length());
  addr_proto->set_port(endpoint.port());

  return addr_proto;
}

}  // namespace collector
