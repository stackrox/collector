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

#include <arpa/inet.h>

#include <array>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

#include <cstring>

#include "Hash.h"

namespace collector {

constexpr size_t kMaxAddrLen = 16;

class Address {
 public:
  enum class Family : unsigned char {
    IPV4,
    IPV6,
  };

  Address(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
    : Address(htonl(static_cast<uint32_t>(a) << 24 |
                    static_cast<uint32_t>(b) << 16 |
                    static_cast<uint32_t>(c) << 8 |
                    static_cast<uint32_t>(d))) {}

  Address(uint32_t ipv4) : Address(Family::IPV4) {
    std::memcpy(data_.data(), &ipv4, sizeof(ipv4));
  }

  Address(uint64_t ipv6_low, uint64_t ipv6_high, unsigned short port) : Address(Family::IPV6) {
    std::memcpy(data_.data(), &ipv6_low, sizeof(ipv6_low));
    std::memcpy(data_.data() + sizeof(ipv6_low), &ipv6_high, sizeof(ipv6_high));
  }

  Family family() const { return family_; }

  size_t Hash() const { return HashAll(data_, family_); }

  bool operator==(const Address& other) const {
    return family_ == other.family_ && data_ == other.data_;
  }

  bool operator!=(const Address& other) const {
    return !(*this == other);
  }

 private:
  friend std::ostream& operator<<(std::ostream& os, const Address& addr) {
    int af = (addr.family_ == Family::IPV4) ? AF_INET : AF_INET6;
    char addr_str[INET6_ADDRSTRLEN];
    if (!inet_ntop(af, addr.data_.data(), addr_str, sizeof(addr_str))) {
      return os << "<invalid address>";
    }
    return os << addr_str;
  }

  Address(Family family) : family_(family) {
    std::memset(data_.data(), 0, data_.size());
  }

  std::array<unsigned char, kMaxAddrLen> data_;
  Family family_;
};

class Endpoint {
 public:
  Endpoint(const Address& address, unsigned short port) : address_(address), port_(port) {}

  size_t Hash() const {
    return HashAll(address_, port_);
  }

  bool operator==(const Endpoint& other) const {
    return port_ == other.port_ && address_ == other.address_;
  }

  bool operator!=(const Endpoint& other) const {
    return !(*this == other);
  }

 private:
  friend std::ostream& operator<<(std::ostream& os, const Endpoint& ep) {
    if (ep.address_.family() == Address::Family::IPV6) {
      os << "[" << ep.address_ << "]";
    } else {
      os << ep.address_;
    }
    return os << ":" << ep.port_;
  }

  Address address_;
  uint16_t port_;
};

enum class L4Proto : uint8_t {
  UNKNOWN = 0,
  TCP = 1,
  UDP = 2,
  ICMP = 3,
};

class Connection {
 public:
  Connection(std::string container, const Endpoint& server, const Endpoint& client, L4Proto l4proto, bool is_server)
    : container_(std::move(container)), server_(server), client_(client), flags_(static_cast<uint8_t>(l4proto) << 1 | (is_server) ? 1 : 0)
  {}

  const std::string& container() const { return container_; }
  const Endpoint& server() const { return server_; }
  const Endpoint& client() const { return client_; }
  bool IsServer() const { return flags_ & 0x1; }
  L4Proto l4proto() const { return static_cast<L4Proto>(flags_ >> 1); }

  bool operator==(const Connection& other) const {
    return container_ == other.container_ && server_ == other.server_ && client_ == other.client_ && flags_ == other.flags_;
  }

  bool operator!=(const Connection& other) const {
    return !(*this == other);
  }

  size_t Hash() const { return HashAll(container_, server_, client_, flags_); }

 private:
  std::string container_;
  Endpoint server_;
  Endpoint client_;
  uint8_t flags_;
};

// We store the connection state as an int64_t microtimestamp, with the least significant bit used as an indicator
// whether the connection is active (1) or not (0).
using ConnMap = UnorderedMap<Connection, int64_t>;

class ConnectionTracker {
 public:
  void AddConnection(const Connection& conn, int64_t timestamp);
  void RemoveConnection(const Connection& conn, int64_t timestamp);

  void Update(const std::vector<Connection>& all_conns, int64_t timestamp);

  // Atomically fetch a snapshot of the current state, removing all inactive connections if requested.
  ConnMap FetchState(bool clear_inactive = true);

  // ComputeDelta computes a diff between new_state and *old_state, and stores the diff in *old_state.
  static void ComputeDelta(const ConnMap& new_state, ConnMap* old_state);

  // Timestamp + active-flag manipulation functions.
  static bool IsActive(int64_t ts) { return ts & 0x1; }
  static void MakeInactive(int64_t* ts) { *ts &= ~0x1; }
  static int64_t MakeActive(int64_t ts) { return ts | 0x1; }
  static int64_t MakeInactive(int64_t ts) { return ts & ~0x1; }

 private:
  void EmplaceOrUpdate(const Connection& conn, int64_t ts);

  std::mutex mutex_;
  ConnMap state_;
};

}  // namespace collector

#endif //COLLECTOR_CONNTRACKER_H
