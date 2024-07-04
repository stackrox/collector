#include "ConnTracker.h"

#include <utility>

#include "CollectorStats.h"
#include "Containers.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

namespace {

static const Address canonical_external_ipv4_addr(255, 255, 255, 255);
static const Address canonical_external_ipv6_addr(0xffffffffffffffffULL, 0xffffffffffffffffULL);
static const NRadixTree private_networks_tree(PrivateNetworks());

}  // namespace

bool AdvertisedEndpointEquality::operator()(const ContainerEndpoint& lhs, const ContainerEndpoint& rhs) const {
  if (bool(lhs.originator()) != bool(rhs.originator())) {
    return false;
  }

  if (lhs.originator()) {
    auto& lhs_originator = *lhs.originator();
    auto& rhs_originator = *rhs.originator();

    /* Here is the real difference with the comparator in ContainerEndpointMap.
       We only compare attributes that are part of the serialized originator process object: storage::NetworkProcessUniqueKey */
    if ((lhs_originator.comm() != rhs_originator.comm()) || (lhs_originator.exe_path() != rhs_originator.exe_path()) || (lhs_originator.args() != rhs_originator.args())) {
      return false;
    }
  }

  return lhs.container() == rhs.container() && lhs.endpoint() == rhs.endpoint() && lhs.l4proto() == rhs.l4proto();
}

bool ContainsPrivateNetwork(Address::Family family, NRadixTree tree) {
  return tree.IsAnyIPNetSubset(family, private_networks_tree) || private_networks_tree.IsAnyIPNetSubset(family, tree);
}

void ConnectionTracker::UpdateConnection(const Connection& conn, int64_t timestamp, bool added) {
  WITH_LOCK(mutex_) {
    EmplaceOrUpdateNoLock(conn, ConnStatus(timestamp, added));
  }
}

void ConnectionTracker::Update(
    const std::vector<Connection>& all_conns,
    const std::vector<ContainerEndpoint>& all_listen_endpoints,
    int64_t timestamp) {
  WITH_LOCK(mutex_) {
    // Mark all existing connections and listen endpoints as inactive
    for (auto& prev_conn : conn_state_) {
      prev_conn.second.SetActive(false);
    }
    for (auto& prev_endpoint : endpoint_state_) {
      prev_endpoint.second.SetActive(false);
    }

    ConnStatus new_status(timestamp, true);

    // Insert (or mark as active) all current connections and listen endpoints.
    for (const auto& curr_conn : all_conns) {
      EmplaceOrUpdateNoLock(curr_conn, new_status);
    }
    for (const auto& curr_endpoint : all_listen_endpoints) {
      EmplaceOrUpdateNoLock(curr_endpoint, new_status);
    }
  }
}

IPNet ConnectionTracker::NormalizeAddressNoLock(const Address& address) const {
  if (address.IsNull()) {
    return {};
  }

  bool private_addr = !address.IsPublic();
  bool do_not_aggregate_addr = !non_aggregated_networks_.Find(address).IsNull();

  // We want to keep private addresses and explicitely requested ones.
  bool keep_addr = private_addr || do_not_aggregate_addr;

  const bool* known_private_networks_exists = Lookup(known_private_networks_exists_, address.family());
  if (keep_addr && (known_private_networks_exists && !*known_private_networks_exists)) {
    return IPNet(address, 0, true);
  }

  const auto& network = known_ip_networks_.Find(address);
  if (keep_addr || Contains(known_public_ips_, address)) {
    return IPNet(address, network.bits(), true);
  }

  if (!network.IsNull()) {
    return network;
  }

  if (enable_external_ips_) {
    return IPNet(address, address.length() * 8);
  }

  // Otherwise, associate it to "rest of the internet".
  switch (address.family()) {
    case Address::Family::IPV4:
      return IPNet(canonical_external_ipv4_addr, 0, true);
    case Address::Family::IPV6:
      return IPNet(canonical_external_ipv6_addr, 0, true);
    default:
      return {};
  }
}

Connection ConnectionTracker::NormalizeConnectionNoLock(const Connection& conn) const {
  bool is_server = conn.is_server();
  if (conn.l4proto() == L4Proto::UDP) {
    // Inference of server role is unreliable for UDP, so go by port.
    is_server = IsEphemeralPort(conn.remote().port()) > IsEphemeralPort(conn.local().port());
  }

  Endpoint local, remote = conn.remote();

  if (is_server) {
    // If this is the server, only the local port is relevant, while the remote port does not matter.
    local = Endpoint(IPNet(Address()), conn.local().port());
    remote = Endpoint(NormalizeAddressNoLock(conn.remote().address()), 0);
  } else {
    // If this is the client, the local port and address are not relevant.
    local = Endpoint();
    remote = Endpoint(NormalizeAddressNoLock(remote.address()), remote.port());
  }

  return Connection(conn.container(), local, remote, conn.l4proto(), is_server);
}

namespace {

/* return: true if the element has been added */
template <typename T>
bool EmplaceOrUpdate(UnorderedMap<T, ConnStatus>* m, const T& obj, ConnStatus status) {
  auto emplace_res = m->emplace(obj, status);
  if (!emplace_res.second && status.LastActiveTime() > emplace_res.first->second.LastActiveTime()) {
    emplace_res.first->second = status;
  }
  return emplace_res.second;
}

}  // namespace

void ConnectionTracker::EmplaceOrUpdateNoLock(const Connection& conn, ConnStatus status) {
  COUNTER_INC(CollectorStats::net_conn_updates);
  if (EmplaceOrUpdate(&conn_state_, conn, status)) {
    IncrementConnectionStats(conn, inserted_connections_counters_);
  }
}

void ConnectionTracker::EmplaceOrUpdateNoLock(const ContainerEndpoint& ep, ConnStatus status) {
  COUNTER_INC(CollectorStats::net_cep_updates);
  EmplaceOrUpdate(&endpoint_state_, ep, status);
}

namespace {

struct dont_normalize {
  template <typename T>
  inline auto operator()(T&& arg) const -> decltype(std::forward<T>(arg)) {
    return std::forward<T>(arg);
  }
};

struct dont_filter {
  template <typename T>
  inline constexpr bool operator()(T&& arg) const {
    return true;
  }
};

template <typename T, typename ProcessFn, typename FilterFn, typename E = std::equal_to<T>>
UnorderedMap<T, ConnStatus, E> FetchState(UnorderedMap<T, ConnStatus>* state, bool clear_inactive,
                                          const ProcessFn& process_fn, const FilterFn& filter_fn) {
  constexpr bool normalize = !std::is_same<ProcessFn, dont_normalize>::value;
  constexpr bool filter = !std::is_same<FilterFn, dont_filter>::value;

  UnorderedMap<T, ConnStatus, E> fetched_state;

  for (auto it = state->begin(); it != state->end();) {
    const auto& entry = *it;

    if (!filter || filter_fn(entry.first)) {
      if (normalize) {
        auto emplace_res = fetched_state.emplace(process_fn(entry.first), entry.second);
        if (!emplace_res.second) {
          emplace_res.first->second.MergeFrom(entry.second);
        }
      } else {
        fetched_state.insert(entry);
      }
    }

    if (clear_inactive && !entry.second.IsActive()) {
      it = state->erase(it);
    } else {
      ++it;
    }
  }

  return fetched_state;
}

}  // namespace

ConnMap ConnectionTracker::FetchConnState(bool normalize, bool clear_inactive) {
  ConnMap cm;
  size_t state_size;
  WITH_LOCK(mutex_) {
    state_size = conn_state_.size();
    if (HasConnectionFilters()) {
      if (normalize) {
        cm = FetchState(
            &conn_state_, clear_inactive,
            [this](const Connection& conn) { return this->NormalizeConnectionNoLock(conn); },
            [this](const Connection& conn) { return this->ShouldFetchConnection(conn); });
      } else {
        cm = FetchState(&conn_state_, clear_inactive, dont_normalize(),
                        [this](const Connection& conn) { return this->ShouldFetchConnection(conn); });
      }
    } else {
      if (normalize) {
        cm = FetchState(
            &conn_state_, clear_inactive,
            [this](const Connection& conn) { return this->NormalizeConnectionNoLock(conn); },
            dont_filter());
      } else {
        cm = FetchState(&conn_state_, clear_inactive, dont_normalize(), dont_filter());
      }
    }
    COUNTER_ADD(CollectorStats::net_conn_inactive, (state_size - conn_state_.size()));
  }
  return cm;
}

AdvertisedEndpointMap ConnectionTracker::FetchEndpointState(bool normalize, bool clear_inactive) {
  AdvertisedEndpointMap cem;
  size_t state_size;
  WITH_LOCK(mutex_) {
    state_size = conn_state_.size();
    if (HasConnectionFilters()) {
      if (normalize) {
        cem = FetchState<ContainerEndpoint, std::function<ContainerEndpoint(const ContainerEndpoint&)>, std::function<bool(const ContainerEndpoint&)>, AdvertisedEndpointEquality>(
            &endpoint_state_, clear_inactive,
            [this](const ContainerEndpoint& cep) { return this->NormalizeContainerEndpoint(cep); },
            [this](const ContainerEndpoint& cep) { return this->ShouldFetchContainerEndpoint(cep); });
      } else {
        cem = FetchState<ContainerEndpoint, dont_normalize, std::function<bool(const ContainerEndpoint&)>, AdvertisedEndpointEquality>(
            &endpoint_state_, clear_inactive,
            dont_normalize(),
            [this](const ContainerEndpoint& cep) { return this->ShouldFetchContainerEndpoint(cep); });
      }
    } else {
      if (normalize) {
        cem = FetchState<ContainerEndpoint, std::function<ContainerEndpoint(const ContainerEndpoint&)>, dont_filter, AdvertisedEndpointEquality>(
            &endpoint_state_, clear_inactive,
            [this](const ContainerEndpoint& cep) { return this->NormalizeContainerEndpoint(cep); },
            dont_filter());
      } else {
        cem = FetchState<ContainerEndpoint, dont_normalize, dont_filter, AdvertisedEndpointEquality>(
            &endpoint_state_, clear_inactive,
            dont_normalize(),
            dont_filter());
      }
    }
    COUNTER_ADD(CollectorStats::net_cep_inactive, (state_size - endpoint_state_.size()));
  }
  return cem;
}

void ConnectionTracker::UpdateKnownPublicIPs(collector::UnorderedSet<collector::Address>&& known_public_ips) {
  COUNTER_SET(CollectorStats::net_known_public_ips, known_public_ips.size());
  WITH_LOCK(mutex_) {
    known_public_ips_ = std::move(known_public_ips);
    if (CLOG_ENABLED(DEBUG)) {
      CLOG(DEBUG) << "known public ips:";
      for (const auto& public_ip : known_public_ips_) {
        CLOG(DEBUG) << " - " << public_ip;
      }
    }
  }
}

void ConnectionTracker::UpdateKnownIPNetworks(UnorderedMap<Address::Family, std::vector<IPNet>>&& known_ip_networks) {
  NRadixTree tree;
  for (const auto& network_pair : known_ip_networks) {
    for (const auto& network : network_pair.second) {
      if (!tree.Insert(network)) {
        // Log error and continue inserting rest of networks.
        CLOG(ERROR) << "Failed to insert CIDR " << network << " in network tree";
      }
    }
  }

  UnorderedMap<Address::Family, bool> known_private_networks_exists;
  COUNTER_ZERO(CollectorStats::net_known_ip_networks);
  for (const auto& network_pair : known_ip_networks) {
    COUNTER_ADD(CollectorStats::net_known_ip_networks, network_pair.second.size());
    known_private_networks_exists[network_pair.first] = ContainsPrivateNetwork(network_pair.first, tree);
  }

  WITH_LOCK(mutex_) {
    known_ip_networks_ = tree;
    known_private_networks_exists_ = std::move(known_private_networks_exists);
    if (CLOG_ENABLED(DEBUG)) {
      CLOG(DEBUG) << "known ip networks:";
      for (auto network : known_ip_networks_.GetAll()) {
        CLOG(DEBUG) << " - " << network;
      }
    }
  }
}

void ConnectionTracker::UpdateIgnoredL4ProtoPortPairs(UnorderedSet<L4ProtoPortPair>&& ignored_l4proto_port_pairs) {
  WITH_LOCK(mutex_) {
    ignored_l4proto_port_pairs_ = std::move(ignored_l4proto_port_pairs);
    if (CLOG_ENABLED(DEBUG)) {
      CLOG(DEBUG) << "ignored l4 protocol and port pairs";
      for (const auto& proto_port_pair : ignored_l4proto_port_pairs_) {
        CLOG(DEBUG) << proto_port_pair.first << "/" << proto_port_pair.second;
      }
    }
  }
}

void ConnectionTracker::UpdateIgnoredNetworks(const std::vector<IPNet>& network_list) {
  WITH_LOCK(mutex_) {
    ignored_networks_ = NRadixTree(network_list);
  }
}

void ConnectionTracker::UpdateNonAggregatedNetworks(const std::vector<IPNet>& network_list) {
  WITH_LOCK(mutex_) {
    non_aggregated_networks_ = NRadixTree(network_list);
  }
}

// Increment the stat counter matching the connection's characteristics
inline void ConnectionTracker::IncrementConnectionStats(Connection conn, ConnectionTracker::Stats& stats) const {
  auto& direction = conn.is_server() ? stats.inbound : stats.outbound;

  if (!ShouldFetchConnection(conn)) {
    // This connection will not be sent, so don't count it.
    return;
  }

  if (conn.remote().address().IsPublic()) {
    direction.public_++;
  } else {
    direction.private_++;
  }
}

// Retrieve the number of connections currently known to ConnTracker, indexed by in/out and public/private nature.
ConnectionTracker::Stats ConnectionTracker::GetConnectionStats_StoredConnections() {
  ConnectionTracker::Stats stats = {};

  WITH_LOCK(mutex_) {
    for (auto& conn : conn_state_) {
      IncrementConnectionStats(conn.first, stats);
    }
  }

  return stats;
}

// Retrieve the value of the ever-increasing counters of new connection insertion, indexed by in/out and public/private nature.
ConnectionTracker::Stats ConnectionTracker::GetConnectionStats_NewConnectionCounters() {
  ConnectionTracker::Stats stats;

  WITH_LOCK(mutex_) {
    stats = inserted_connections_counters_;
  }
  return stats;
}
}  // namespace collector
