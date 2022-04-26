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

#ifndef COLLECTOR_CONNTRACKER_H
#define COLLECTOR_CONNTRACKER_H

#include <mutex>
#include <vector>

#include "Containers.h"
#include "Hash.h"
#include "NRadix.h"
#include "NetworkConnection.h"

namespace collector {

// ConnStatus encapsulates the status of a connection, comprised of the timestamp when the connection was last seen
// alive (in microseconds since epoch), and a flag indicating whether the connection is currently active.
class ConnStatus {
 private:
  static constexpr uint64_t kActiveFlag = 1UL << 63;

  static inline uint64_t MakeActive(uint64_t data, bool active) {
    return active ? (data | kActiveFlag) : (data & ~kActiveFlag);
  }

 public:
  ConnStatus() : data_(0UL) {}
  ConnStatus(int64_t microtimestamp, bool active) : data_(MakeActive(static_cast<uint64_t>(microtimestamp), active)) {}

  int64_t LastActiveTime() const { return static_cast<int64_t>(data_ & ~kActiveFlag); }
  bool IsActive() const { return (data_ & kActiveFlag) != 0; }

  void SetActive(bool active) {
    data_ = MakeActive(data_, active);
  }

  void MergeFrom(const ConnStatus& other) {
    data_ = std::max(data_, other.data_);
  }

  ConnStatus WithStatus(bool active) const {
    return ConnStatus(MakeActive(data_, active));
  }

  bool operator==(const ConnStatus& other) const {
    return data_ == other.data_;
  }

  bool operator!=(const ConnStatus& other) const {
    return !(*this == other);
  }

  // Returns true if a connection was active during the afterglow period.
  // This is helpful for not reporting frequent connections every time we see them.
  bool IsInAfterglowPeriod(int64_t time_micros, int64_t afterglow_period_micros) const {
    return time_micros - LastActiveTime() < afterglow_period_micros;
  }

  // Returns true if a connection is active or was active during the afterglow period.
  // This is helpful for not reporting frequent connections every time we see them.
  bool WasRecentlyActive(int64_t time_micros, int64_t afterglow_period_micros) const {
    return IsActive() || IsInAfterglowPeriod(time_micros, afterglow_period_micros);
  }

 private:
  explicit ConnStatus(uint64_t data) : data_(data) {}

  uint64_t data_;
};

using ConnMap = UnorderedMap<Connection, ConnStatus>;
using ContainerEndpointMap = UnorderedMap<ContainerEndpoint, ConnStatus>;

class CollectorStats;

class ConnectionTracker {
 public:
  void UpdateConnection(const Connection& conn, int64_t timestamp, bool added);
  void AddConnection(const Connection& conn, int64_t timestamp) {
    UpdateConnection(conn, timestamp, true);
  }
  void RemoveConnection(const Connection& conn, int64_t timestamp) {
    UpdateConnection(conn, timestamp, false);
  }

  void Update(const std::vector<Connection>& all_conns, const std::vector<ContainerEndpoint>& all_listen_endpoints, int64_t timestamp);

  // Atomically fetch a snapshot of the current state, removing all inactive connections if requested.
  ConnMap FetchConnState(bool normalize = false, bool clear_inactive = true);
  ContainerEndpointMap FetchEndpointState(bool normalize = false, bool clear_inactive = true);

  template <typename T>
  static void UpdateOldState(UnorderedMap<T, ConnStatus>* old_state, const UnorderedMap<T, ConnStatus>& new_state, int64_t time_micros, int64_t afterglow_period_micros);

  // ComputeDelta computes a diff between new_state and old_state
  template <typename T>
  static void ComputeDeltaAfterglow(const UnorderedMap<T, ConnStatus>& new_state, const UnorderedMap<T, ConnStatus>& old_state, UnorderedMap<T, ConnStatus>& delta, int64_t time_micros, int64_t time_at_last_scrape, int64_t afterglow_period_micros);

  template <typename T>
  static void AddToAllCep(UnorderedMap<string, ConnStatus>* all_cep, UnorderedMap<T, ConnStatus> delta);

  template <typename T>
  static void PrintConnections(const UnorderedMap<T, ConnStatus> conns);

  // Handles the case when a connection appears in both the new and old states and afterglow is used
  template <typename T>
  void static ComputeDeltaForAConnectionInOldAndNewStates(const std::pair<const T, ConnStatus>& new_conn, const ConnStatus& old_conn_status, UnorderedMap<T, ConnStatus>& delta, int64_t time_micros, int64_t time_at_last_scrape, int64_t afterglow_period_micros);

  // Determines if a connection being added to the delta should be set to active
  template <typename T>
  static std::pair<T, ConnStatus> ChangeConnToActiveIfNeeded(const std::pair<const T, ConnStatus>& new_conn, const T& conn_key, const ConnStatus& conn_status, bool new_recently_active);

  // Handles the case when a connection appears in only the new state and afterglow is used
  template <typename T>
  void static ComputeDeltaForAConnectionInNewState(const std::pair<const T, ConnStatus>& new_conn, UnorderedMap<T, ConnStatus>& delta, int64_t time_micros, int64_t afterglow_period_micros);

  // Determines if an old connection should be reported as being inactive
  template <typename T>
  static bool CheckIfOldConnShouldBeInactiveInDelta(const T& conn_key, const ConnStatus& conn_status, const UnorderedMap<T, ConnStatus>& new_state, int64_t time_micros, int64_t time_at_last_scrape, int64_t afterglow_period_micros);

  // ComputeDelta computes a diff between new_state and *old_state, and stores the diff in *old_state.
  template <typename T>
  static void ComputeDelta(const UnorderedMap<T, ConnStatus>& new_state, UnorderedMap<T, ConnStatus>* old_state);

  void UpdateKnownPublicIPs(UnorderedSet<Address>&& known_public_ips);
  void UpdateKnownIPNetworks(UnorderedMap<Address::Family, std::vector<IPNet>>&& known_ip_networks);
  void UpdateIgnoredL4ProtoPortPairs(UnorderedSet<L4ProtoPortPair>&& ignored_l4proto_port_pairs);

 private:
  // NormalizeConnection transforms a connection into a normalized form.
  Connection NormalizeConnectionNoLock(const Connection& conn) const;

  // Emplace a connection into the state ConnMap, or update its timestamp if the supplied timestamp is more recent
  // than the stored one.
  void EmplaceOrUpdateNoLock(const Connection& conn, ConnStatus status);

  // Emplace a listen endpoint into the state ContainerEndpointMap, or update its timestamp if the supplied timestamp is more
  // recent than the stored one.
  void EmplaceOrUpdateNoLock(const ContainerEndpoint& ep, ConnStatus status);

  IPNet NormalizeAddressNoLock(const Address& address) const;

  // Returns true if any connection filters are found.
  inline bool HasConnectionStateFilters() const {
    return !ignored_l4proto_port_pairs_.empty();
  }

  // Determine if a protocol port combination from a connection or endpoint should be ignored
  inline bool IsIgnoredL4ProtoPortPair(const L4ProtoPortPair& p) const {
    return Contains(ignored_l4proto_port_pairs_, p);
  }

  // NormalizeContainerEndpoint transforms a container endpoint into a normalized form.
  inline ContainerEndpoint NormalizeContainerEndpoint(const ContainerEndpoint& cep) const {
    const auto& ep = cep.endpoint();
    return ContainerEndpoint(cep.container(), Endpoint(Address(ep.address().family()), ep.port()), cep.l4proto());
  }

  // Determine if a connection should be ignored
  inline bool ShouldFetchConnection(const Connection& conn) const {
    return !IsIgnoredL4ProtoPortPair(L4ProtoPortPair(conn.l4proto(), conn.local().port())) &&
           !IsIgnoredL4ProtoPortPair(L4ProtoPortPair(conn.l4proto(), conn.remote().port()));
  }

  // Determine if a container endpoint should be ignored
  inline bool ShouldFetchContainerEndpoint(const ContainerEndpoint& cep) const {
    return !IsIgnoredL4ProtoPortPair(L4ProtoPortPair(cep.l4proto(), cep.endpoint().port()));
  }

  std::mutex mutex_;
  ConnMap conn_state_;
  ContainerEndpointMap endpoint_state_;

  UnorderedSet<Address> known_public_ips_;
  NRadixTree known_ip_networks_;
  UnorderedMap<Address::Family, bool> known_private_networks_exists_;
  UnorderedSet<L4ProtoPortPair> ignored_l4proto_port_pairs_;
};

/* static */
template <typename T>
void ConnectionTracker::UpdateOldState(UnorderedMap<T, ConnStatus>* old_state, const UnorderedMap<T, ConnStatus>& new_state, int64_t time_micros, int64_t afterglow_period_micros) {
  // Remove inactive connections that are older than the afterglow period and add unexpired new connections to the old state
  for (auto it = old_state->begin(); it != old_state->end();) {
    auto& old_conn = *it;
    if (old_conn.second.WasRecentlyActive(time_micros, afterglow_period_micros)) {
      ++it;
    } else {
      it = old_state->erase(it);
    }
  }

  for (const auto& conn : new_state) {
    auto insert_res = old_state->insert(conn);
    if (!insert_res.second) {  // Was already present. Update the connection.
      auto& old_conn = *insert_res.first;
      old_conn.second = conn.second;
    }
  }
}

template <typename T>
void ConnectionTracker::ComputeDelta(const UnorderedMap<T, ConnStatus>& new_state, UnorderedMap<T, ConnStatus>* old_state) {
  CLOG(INFO) << "Print new_conn_state";
  PrintConnections(new_state);
  CLOG(INFO) << "Print old_conn_state";
  PrintConnections(*old_state);
  // Insert all objects from the new state, if anything changed about them.
  for (const auto& conn : new_state) {
    auto insert_res = old_state->insert(conn);
    auto& old_conn = *insert_res.first;
    if (!insert_res.second) {  // was already present
      if (conn.second.IsActive() != old_conn.second.IsActive()) {
        // Object was either resurrected or newly closed. Update in either case.
        old_conn.second = conn.second;
      } else if (conn.second.IsActive()) {
        // Both objects are active. Not part of the delta.
        old_state->erase(insert_res.first);
      } else {
        // Both objects are inactive. Update the timestamp if applicable, otherwise omit from delta.
        CLOG(INFO) << "old_conn.second.LastActiveTime()= " << old_conn.second.LastActiveTime() << " conn.second.LastActiveTime()= " << conn.second.LastActiveTime();
        if (old_conn.second.LastActiveTime() < conn.second.LastActiveTime()) {
          old_conn.second = conn.second;
          CLOG(INFO) << "Updated timestamp";
        } else {
          old_state->erase(insert_res.first);
        }
      }
    }
  }

  // Mark all active objects in the old state that are not present in the new state as inactive, and remove the
  // inactive ones.
  for (auto it = old_state->begin(); it != old_state->end();) {
    auto& old_conn = *it;
    // Ignore all objects present in the new state.
    if (new_state.find(old_conn.first) != new_state.end()) {
      ++it;
      continue;
    }

    if (old_conn.second.IsActive()) {
      old_conn.second.SetActive(false);
      ++it;
    } else {
      it = old_state->erase(it);
    }
  }
  CLOG(INFO) << "Print old_conn_state";
  PrintConnections(*old_state);
}

// This function takes in old network connections or endpoints (old_state) and the
// connections that occurred in the last scrape interval (new_state) and returns
// their difference or delta, which is then reported in NetworkStatusNotifier.cpp.
// This also uses afterglow meaning that connection which were active within an
// afterglow period (afterglow_period_micros) are treated as being active.
// This function also takes in the time of the current scrape (time_micros) and
// the time at the previous scrape (time_at_last_scrape). These are used to determine
// if the new connections were active within the afterglow period of the current scrape
// and if the old_connection were active within the afterglow period of the previous scrape
template <typename T>
void ConnectionTracker::ComputeDeltaAfterglow(const UnorderedMap<T, ConnStatus>& new_state,
                                              const UnorderedMap<T, ConnStatus>& old_state,
                                              UnorderedMap<T, ConnStatus>& delta,
                                              int64_t time_micros,
                                              int64_t time_at_last_scrape,
                                              int64_t afterglow_period_micros) {
  // Insert all objects from the new state, if anything changed about them.
  for (const auto& new_conn : new_state) {
    auto& conn_key = new_conn.first;
    auto old_conn = old_state.find(conn_key);
    // Was already present
    if (old_conn != old_state.end()) {
      ComputeDeltaForAConnectionInOldAndNewStates(new_conn, old_conn->second, delta, time_micros, time_at_last_scrape, afterglow_period_micros);
    } else {
      ComputeDeltaForAConnectionInNewState(new_conn, delta, time_micros, afterglow_period_micros);
    }
  }

  // Add everything in the old state that was in the active state and is not in the new state
  for (const auto& old_conn : old_state) {
    auto& conn_key = old_conn.first;
    auto& conn_status = old_conn.second;

    if (CheckIfOldConnShouldBeInactiveInDelta(conn_key, conn_status, new_state, time_micros, time_at_last_scrape, afterglow_period_micros)) {
      delta.insert(std::make_pair(conn_key, ConnStatus(conn_status.LastActiveTime(), false)));
    }
  }
}

// See ComputeDeltaAfterglow
// Handles the case when a connection appears in both the new and old states and afterglow is used
template <typename T>
inline void ConnectionTracker::ComputeDeltaForAConnectionInOldAndNewStates(const std::pair<const T, ConnStatus>& new_conn,
                                                                           const ConnStatus& old_conn_status,
                                                                           UnorderedMap<T, ConnStatus>& delta,
                                                                           int64_t time_micros,
                                                                           int64_t time_at_last_scrape,
                                                                           int64_t afterglow_period_micros) {
  auto& conn_key = new_conn.first;
  auto& conn_status = new_conn.second;

  //Connections active within the afterglow period are considered to be active for the purpose of the delta.
  bool new_recently_active = conn_status.WasRecentlyActive(time_micros, afterglow_period_micros);
  bool old_recently_active = old_conn_status.WasRecentlyActive(time_at_last_scrape, afterglow_period_micros);

  if (new_recently_active != old_recently_active) {
    auto new_delta = ChangeConnToActiveIfNeeded(new_conn, conn_key, conn_status, new_recently_active);
    delta.insert(new_delta);
  } else if (!new_recently_active) {
    // Both objects are inactive. Include the new status in the delta if it has shown activity more recently than in the old_state
    if (old_conn_status.LastActiveTime() < conn_status.LastActiveTime()) {
      delta.insert(new_conn);
    }
  }
}

// Invoked when the connection does not exist in the old state or is inactive in the old state. In this the connection should be set to
// active if it was active within the afterglow period
template <typename T>
inline std::pair<T, ConnStatus> ConnectionTracker::ChangeConnToActiveIfNeeded(const std::pair<const T, ConnStatus>& new_conn,
                                                                              const T& conn_key,
                                                                              const ConnStatus& conn_status,
                                                                              bool new_recently_active) {
  if (new_recently_active && !conn_status.IsActive()) {
    return std::make_pair(conn_key, ConnStatus(conn_status.LastActiveTime(), true));
  }

  return new_conn;
}

// See ComputeDeltaAfterglow
// Handles the case when a connection appears in only the new state and afterglow is used
template <typename T>
inline void ConnectionTracker::ComputeDeltaForAConnectionInNewState(const std::pair<const T, ConnStatus>& new_conn,
                                                                    UnorderedMap<T, ConnStatus>& delta,
                                                                    int64_t time_micros,
                                                                    int64_t afterglow_period_micros) {
  auto& conn_key = new_conn.first;
  auto& conn_status = new_conn.second;

  bool new_recently_active = conn_status.WasRecentlyActive(time_micros, afterglow_period_micros);
  auto new_delta = ChangeConnToActiveIfNeeded(new_conn, conn_key, conn_status, new_recently_active);
  delta.insert(new_delta);
}

// If the connection is in the old_state, not in the new state, was active within the afterglow period of the previous scrape, and is now outside of the
// afterglow period of the current state, it should be reported as being inactive.
template <typename T>
bool ConnectionTracker::CheckIfOldConnShouldBeInactiveInDelta(const T& conn_key,
                                                              const ConnStatus& conn_status,
                                                              const UnorderedMap<T, ConnStatus>& new_state,
                                                              int64_t time_micros,
                                                              int64_t time_at_last_scrape,
                                                              int64_t afterglow_period_micros) {
  bool is_old_conn_in_new_state = (new_state.find(conn_key) != new_state.end());
  if (is_old_conn_in_new_state) {
    return false;
  }

  bool old_recently_active = conn_status.WasRecentlyActive(time_at_last_scrape, afterglow_period_micros);
  if (!old_recently_active) {
    return false;
  }

  bool recently_active_at_time_micros = conn_status.IsInAfterglowPeriod(time_micros, afterglow_period_micros);
  if (recently_active_at_time_micros) {
    return false;
  }

  return true;
}

template <typename T>
void ConnectionTracker::PrintConnections(const UnorderedMap<T, ConnStatus> conns) {
  for (auto conn : conns) {
    CLOG(INFO) << conn.first << "\t" << conn.second.LastActiveTime() << conn.second.IsActive();
  }
  CLOG(INFO) << " ";
  CLOG(INFO) << " ";
  CLOG(INFO) << " ";
}

template <typename T>
void ConnectionTracker::AddToAllCep(UnorderedMap<string, ConnStatus>* all_cep, UnorderedMap<T, ConnStatus> delta) {
  CLOG(INFO) << "delta";
  PrintConnections(delta);
  for (auto cep : delta) {
    auto& cep_key = cep.first;
    std::stringstream ss;
    ss << cep_key;
    string cep_key_string, port_string, ip_string;
    ss >> cep_key_string;
    ss >> port_string;
    ss >> ip_string;
    cep_key_string += port_string;
    cep_key_string += ip_string;
    std::stringstream lastActiveSS;
    lastActiveSS << cep.second.LastActiveTime();
    string lastActiveTimeString;
    lastActiveSS >> lastActiveTimeString;
    cep_key_string += " " + lastActiveTimeString;
    auto delta_cep = all_cep->find(cep_key_string);
    if (delta_cep != all_cep->end()) {
      CLOG(INFO) << "Already sent " << delta_cep->second.LastActiveTime() << "\t" << delta_cep->first << "\t" << delta_cep->second.IsActive() << "\t cep_key_string= " << cep_key_string;
    } else {
      CLOG(INFO) << "First sent " << cep.second.LastActiveTime() << "\t" << cep.first << "\t" << cep.second.IsActive() << "\t cep_key_string= " << cep_key_string;
      auto temp = std::make_pair(cep_key_string, cep.second);
      all_cep->insert(temp);
    }
  }
}

}  // namespace collector

#endif  //COLLECTOR_CONNTRACKER_H
