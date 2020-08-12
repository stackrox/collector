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

#include "Hash.h"
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

 private:
  explicit ConnStatus(uint64_t data) : data_(data) {}

  uint64_t data_;
};

using ConnMap = UnorderedMap<Connection, ConnStatus>;
using ContainerEndpointMap = UnorderedMap<ContainerEndpoint, ConnStatus>;

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

  // ComputeDelta computes a diff between new_state and *old_state, and stores the diff in *old_state.
  template <typename T>
  static void ComputeDelta(const UnorderedMap<T, ConnStatus>& new_state, UnorderedMap<T, ConnStatus>* old_state);

  void UpdateKnownPublicIPs(UnorderedSet<Address>&& known_public_ips);

 private:
  // NormalizeConnection transforms a connection into a normalized form.
  Connection NormalizeConnectionNoLock(const Connection &conn) const;

  // Emplace a connection into the state ConnMap, or update its timestamp if the supplied timestamp is more recent
  // than the stored one.
  void EmplaceOrUpdateNoLock(const Connection& conn, ConnStatus status);

  // Emplace a listen endpoint into the state ContainerEndpointMap, or update its timestamp if the supplied timestamp is more
  // recent than the stored one.
  void EmplaceOrUpdateNoLock(const ContainerEndpoint& ep, ConnStatus status);

  Address NormalizeAddressNoLock(const Address& address) const;

  std::mutex mutex_;
  ConnMap conn_state_;
  ContainerEndpointMap endpoint_state_;

  UnorderedSet<Address> known_public_ips_;
};

/* static */
template <typename T>
void ConnectionTracker::ComputeDelta(const UnorderedMap<T, ConnStatus>& new_state, UnorderedMap<T, ConnStatus>* old_state) {
  // Insert all objects from the new state, if anything changed about them.
  for (const auto& conn : new_state) {
    auto insert_res = old_state->insert(conn);
    auto &old_conn = *insert_res.first;
    if (!insert_res.second) {  // was already present
      if (conn.second.IsActive() != old_conn.second.IsActive()) {
        // Object was either resurrected or newly closed. Update in either case.
        old_conn.second = conn.second;
      } else if (conn.second.IsActive()) {
        // Both objects are active. Not part of the delta.
        old_state->erase(insert_res.first);
      } else {
        // Both objects are inactive. Update the timestamp if applicable, otherwise omit from delta.
        if (old_conn.second.LastActiveTime() < conn.second.LastActiveTime()) {
          old_conn.second = conn.second;
        } else {
          old_state->erase(insert_res.first);
        }
      }
    }
  }

  // Mark all active objects in the old state that are not present in the new state as inactive, and remove the
  // inactive ones.
  for (auto it = old_state->begin(); it != old_state->end(); ) {
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
}

}  // namespace collector

#endif //COLLECTOR_CONNTRACKER_H
