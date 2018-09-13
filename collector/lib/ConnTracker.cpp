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

#include "ConnTracker.h"

#include <utility>

#include "Utility.h"

namespace collector {

void ConnectionTracker::AddConnection(const Connection &conn, int64_t timestamp) {
  WITH_LOCK(mutex_) {
    EmplaceOrUpdateNoLock(conn, ConnStatus(timestamp, true));
  }
}

void ConnectionTracker::RemoveConnection(const Connection &conn, int64_t timestamp) {
  WITH_LOCK(mutex_) {
    // Even if a connection is not present, record its closing timestamp, so that we don't discard any potentially
    // useful information.
    EmplaceOrUpdateNoLock(conn, ConnStatus(timestamp, false));
  }
}

void ConnectionTracker::Update(const std::vector<Connection> &all_conns, int64_t timestamp) {
  WITH_LOCK(mutex_) {
    // Mark all existing connections as inactive
    for (auto &prev_conn : state_) {
      prev_conn.second.SetActive(false);
    }

    ConnStatus new_status(timestamp, true);

    // Insert (or mark as active) all current connections.
    for (const auto &curr_conn : all_conns) {
      EmplaceOrUpdateNoLock(curr_conn, new_status);
    }
  }
}

void ConnectionTracker::EmplaceOrUpdateNoLock(const Connection& conn, ConnStatus status) {
  auto emplace_res = state_.emplace(conn, status);
  if (!emplace_res.second && status.LastActiveTime() > emplace_res.first->second.LastActiveTime()) {
    emplace_res.first->second = status;
  }
}

ConnMap ConnectionTracker::FetchState(bool clear_inactive) {
  ConnMap new_state;

  WITH_LOCK(mutex_) {
    if (!clear_inactive) {
      return state_;
    }

    for (const auto& conn : state_) {
      if (conn.second.IsActive()) {
        new_state.insert(conn);
      }
    }

    state_.swap(new_state);
  }

  return new_state;
}

/* static */
void ConnectionTracker::ComputeDelta(const ConnMap& new_state, ConnMap* old_state) {
  // Insert all connections from the new state, if anything changed about them.
  for (const auto& conn : new_state) {
    auto insert_res = old_state->insert(conn);
    auto &old_conn = *insert_res.first;
    if (!insert_res.second) {  // was already present
      if (conn.second.IsActive() != old_conn.second.IsActive()) {
        // Connection was either resurrected or newly closed. Update in either case.
        old_conn.second = conn.second;
      } else if (conn.second.IsActive()) {
        // Both connections are active. Not part of the delta.
        old_state->erase(insert_res.first);
      } else {
        // Both connections are inactive. Update the timestamp if applicable, otherwise omit from delta.
        if (old_conn.second.LastActiveTime() < conn.second.LastActiveTime()) {
          old_conn.second = conn.second;
        } else {
          old_state->erase(insert_res.first);
        }
      }
    }
  }

  // Mark all active connections in the old state that are not present in the new state as inactive, and remove the
  // inactive ones.
  for (auto it = old_state->begin(); it != old_state->end(); ) {
    auto& old_conn = *it;
    // Ignore all connections present in the new state.
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

int IsEphemeralPort(uint16_t port) {
  if (port >= 49152) return 4;  // IANA range
  if (port >= 32768) return 3;  // Modern Linux kernel range
  if (port >= 1025 && port <= 5000) return 2;  // FreeBSD (partial) + Windows <=XP range
  if (port == 1024) return 1;  // FreeBSD
  return 0;  // not ephemeral according to any range
}

bool DetermineRole(const Endpoint& local, const Endpoint& remote, const EndpointSet& listen_endpoints) {
  if (listen_endpoints.find(local) != listen_endpoints.end()) return true;
  Endpoint local_all(Address(), local.port());
  if (listen_endpoints.find(local_all) != listen_endpoints.end()) return true;

  return IsEphemeralPort(remote.port()) > IsEphemeralPort(local.port());
}

}  // namespace collector
