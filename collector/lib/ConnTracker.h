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

#include <array>
#include <mutex>
#include <string>
#include <vector>

#include "Hash.h"

namespace collector {

constexpr size_t kMaxAddrLen = 16;

class Address {
 public:
  size_t Hash() const { return CombineHashes(data_, port_); }

  bool operator==(const Address& other) const {
    return data_ == other.data_ && port_ == other.port_;
  }

  bool operator!=(const Address& other) const {
    return !(*this == other);
  }

 private:
  std::array<unsigned char, kMaxAddrLen> data_;
  unsigned short port_;
};

enum class L4Proto : uint8_t {
  UNKNOWN = 0,
  TCP = 1,
  TCP6 = 2,
  UDP = 3,
  UDP6 = 4,
  ICMP = 5,
  ICMP6 = 6,
};

class Connection {
 public:
  const std::string& container() const { return container_; }
  const Address& server() const { return server_; }
  const Address& client() const { return client_; }
  bool IsServer() const { return flags_ & 0x1; }
  L4Proto l4proto() const { return static_cast<L4Proto>(flags_ >> 1); }

  bool operator==(const Connection& other) const {
    return container_ == other.container_ && server_ == other.server_ && client_ == other.client_ && flags_ == other.flags_;
  }

  bool operator!=(const Connection& other) const {
    return !(*this == other);
  }

  size_t Hash() const { return CombineHashes(container_, server_, client_, flags_); }

 private:
  std::string container_;
  Address server_;
  Address client_;
  uint8_t flags_;
};

using ConnMap = UnorderedMap<Connection, int64_t>;

class ConnectionTracker {
 public:
  void AddConnection(const Connection& conn);
  void RemoveConnection(const Connection& conn);

  void Update(const std::vector<Connection>& all_conns);

  ConnMap FetchState(bool clear_inactive = true);

  static void ComputeDelta(const ConnMap& new_state, ConnMap* old_state);

 private:
  static bool IsActive(int64_t ts) { return ts & 0x1; }
  static void MakeInactive(int64_t* ts) { *ts &= ~0x1; }
  static int64_t MakeActive(int64_t ts) { return ts | 0x1; }
  static int64_t MakeInactive(int64_t ts) { return ts & ~0x1; }

  std::mutex mutex_;
  ConnMap state_;
};

}  // namespace collector

#endif //COLLECTOR_CONNTRACKER_H
