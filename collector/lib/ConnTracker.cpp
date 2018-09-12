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
  SCOPED_LOCK(mutex_);

  EmplaceOrUpdate(conn, MakeActive(timestamp));
}

void ConnectionTracker::RemoveConnection(const Connection &conn, int64_t timestamp) {
  SCOPED_LOCK(mutex_);

  // Even if a connection is not present, record its closing timestamp, so that we don't discard any potentially
  // useful information.
  EmplaceOrUpdate(conn, MakeInactive(timestamp));
}

void ConnectionTracker::Update(const std::vector<Connection> &all_conns, int64_t timestamp) {
  SCOPED_LOCK(mutex_);

  // Mark all existing connections as inactive
  for (auto& prev_conn : state_) {
    MakeInactive(&prev_conn.second);
  }

  int64_t ts = MakeActive(timestamp);

  // Insert (or mark as active) all current connections.
  for (const auto& curr_conn : all_conns) {
    EmplaceOrUpdate(curr_conn, ts);
  }
}

void ConnectionTracker::EmplaceOrUpdate(const Connection& conn, int64_t ts) {
  auto emplace_res = state_.emplace(conn, ts);
  if (!emplace_res.second && ts > emplace_res.first->second) {
    emplace_res.first->second = ts;
  }
}

ConnMap ConnectionTracker::FetchState(bool clear_inactive) {
  SCOPED_LOCK(mutex_);

  if (!clear_inactive) {
    return state_;
  }

  ConnMap new_state;
  for (const auto& conn : state_) {
    if (IsActive(conn.second)) {
      new_state.insert(conn);
    }
  }

  using std::swap;
  swap(state_, new_state);

  return std::move(new_state);
}

/* static */
void ConnectionTracker::ComputeDelta(const ConnMap& new_state, ConnMap* old_state) {
  // Insert all connections from the new state, if anything changed about them.
  for (const auto& conn : new_state) {
    auto insert_res = old_state->insert(conn);
    auto &old_conn = *insert_res.first;
    if (!insert_res.second) {  // was already present
      if (IsActive(conn.second) != IsActive(old_conn.second)) {
        // Connection was either resurrected or newly closed. Update in either case.
        old_conn.second = conn.second;
      } else if (IsActive(conn.second)) {
        // Both connections are active. Not part of the delta.
        old_state->erase(insert_res.first);
      } else {
        // Both connections are inactive. Update the timestamp if applicable, otherwise omit from delta.
        if (old_conn.second < conn.second) {
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

    if (IsActive(old_conn.second)) {
      MakeInactive(&old_conn.second);
      ++it;
    } else {
      it = old_state->erase(it);
    }
  }
}

}  // namespace collector
