#include "NetworkSignalHandler.h"

#include <cstring>
#include <optional>

#include <libsinsp/sinsp.h>

#include "EventMap.h"
#include "system-inspector/EventExtractor.h"

namespace collector {

namespace {

enum class Modifier : uint8_t {
  INVALID = 0,
  ADD,
  REMOVE,
};

EventMap<Modifier> modifiers = {
    {
        {"close<", Modifier::REMOVE},
        {"shutdown<", Modifier::REMOVE},
        {"connect<", Modifier::ADD},
        {"accept<", Modifier::ADD},
        {"accept4<", Modifier::ADD},
        {"getsockopt<", Modifier::ADD},
        {"sendto<", Modifier::ADD},
        {"sendto>", Modifier::ADD},
        {"sendmsg<", Modifier::ADD},
        {"sendmsg>", Modifier::ADD},
        {"sendmmsg<", Modifier::ADD},
        {"recvfrom<", Modifier::ADD},
        {"recvfrom>", Modifier::ADD},
        {"recvmsg<", Modifier::ADD},
        {"recvmsg>", Modifier::ADD},
        {"recvmmsg<", Modifier::ADD},
        {"recvmmsg>", Modifier::ADD},
    },
    Modifier::INVALID,
};

}  // namespace

NetworkSignalHandler::NetworkSignalHandler(sinsp* inspector, std::shared_ptr<ConnectionTracker> conn_tracker, system_inspector::Stats* stats)
    : event_extractor_(std::make_unique<system_inspector::EventExtractor>()), conn_tracker_(std::move(conn_tracker)), stats_(stats), collect_connection_status_(true), track_send_recv_(false) {
  event_extractor_->Init(inspector);
}

NetworkSignalHandler::~NetworkSignalHandler() = default;

/*
 * Socket connection life-cycle scenarii:
 * - synchronous:
 *   - events:  connect()/accept() >= 0 --> close()/shutdown() = 0
 *     fd_info: connected                   connected
 *     result:  ADD                         REMOVE
 *   - events:  connect()/accept() < 0
 *     fd_info: failed
 *     result:  nil
 * - asynchronous:
 *   - events:  connect() < 0 (E_INPROGRESS) --> getsockopt() ok --> shared with synchronous
 *     fd_info: pending                          connected
 *     result:  nil                              ADD
 *   - events:  connect() < 0 (other)
 *     fd_info: failed
 *     result:  nil
 */
std::optional<Connection> NetworkSignalHandler::GetConnection(sinsp_evt* evt) {
  auto* fd_info = evt->get_fd_info();

  if (!fd_info) {
    return std::nullopt;
  }

  // With collect_connection_status_ set, we can prevent reporting of asynchronous
  // connections which fail.
  if (collect_connection_status_) {
    // note: connection status tracking enablement is managed in system_inspector::Service
    if (fd_info->is_socket_failed()) {
      // connect() failed or getsockopt(SO_ERROR) returned a failure
      return std::nullopt;
    }

    if (fd_info->is_socket_pending()) {
      // connect() returned E_INPROGRESS
      return std::nullopt;
    }
  }

  auto res = event_extractor_->get_event_rawres(evt);
  if (!res.has_value() || res.value() < 0) {
    // ignore unsuccessful events for now.
    return std::nullopt;
  }

  bool is_server = fd_info->is_role_server();
  if (!is_server && !fd_info->is_role_client()) {
    return std::nullopt;
  }

  L4Proto l4proto;
  switch (fd_info->get_l4proto()) {
    case SCAP_L4_TCP:
      l4proto = L4Proto::TCP;
      break;
    case SCAP_L4_UDP:
      l4proto = L4Proto::UDP;
      break;
    default:
      return std::nullopt;
  }

  Endpoint client, server;
  switch (fd_info->m_type) {
    case SCAP_FD_IPV4_SOCK: {
      const auto& ipv4_fields = fd_info->m_sockinfo.m_ipv4info.m_fields;
      client = Endpoint(Address(ipv4_fields.m_sip), ipv4_fields.m_sport);
      server = Endpoint(Address(ipv4_fields.m_dip), ipv4_fields.m_dport);
      break;
    }
    case SCAP_FD_IPV6_SOCK: {
      const auto& ipv6_fields = fd_info->m_sockinfo.m_ipv6info.m_fields;
      client = Endpoint(Address(ipv6_fields.m_sip.m_b), ipv6_fields.m_sport);
      server = Endpoint(Address(ipv6_fields.m_dip.m_b), ipv6_fields.m_dport);
      break;
    }
    default:
      return std::nullopt;
  }

  const Endpoint* local = is_server ? &server : &client;
  const Endpoint* remote = is_server ? &client : &server;

  const char* container_id = event_extractor_->get_container_id(evt);
  if (!container_id) {
    return std::nullopt;
  }
  return {Connection(container_id, *local, *remote, l4proto, is_server)};
}

SignalHandler::Result NetworkSignalHandler::HandleSignal(sinsp_evt* evt) {
  auto modifier = modifiers[evt->get_type()];
  if (modifier == Modifier::INVALID) {
    return SignalHandler::IGNORED;
  }

  auto result = GetConnection(evt);
  if (!result.has_value() || !IsRelevantConnection(*result)) {
    return SignalHandler::IGNORED;
  }

  conn_tracker_->UpdateConnection(*result, evt->get_ts() / 1000UL, modifier == Modifier::ADD);
  return SignalHandler::PROCESSED;
}

std::vector<std::string> NetworkSignalHandler::GetRelevantEvents() {
  std::vector<std::string> base_events = {
      "close<",
      "shutdown<",
      "connect<",
      "accept<",
      "accept4<",
      "getsockopt<"};

  if (track_send_recv_) {
    // disable clang-format here because it will massively
    // indent the initializer list.
    //
    // clang-format off
    base_events.insert(base_events.end(), {
        "sendto<",
        "sendto>",
        "sendmsg<",
        "sendmsg>",
        "sendmmsg<",
        "recvfrom<",
        "recvfrom>",
        "recvmsg<",
        "recvmsg>",
        "recvmmsg<",
        "recvmmsg>",
        "recvmsg<",
        "recvmsg>"
    });
    // clang-format on
  }
  return base_events;
}

bool NetworkSignalHandler::Stop() {
  event_extractor_->ClearWrappers();
  return true;
}

}  // namespace collector
