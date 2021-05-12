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

#ifndef COLLECTOR_NETWORKCONNECTION_H
#define COLLECTOR_NETWORKCONNECTION_H

#include <arpa/inet.h>
#include <endian.h>

#define htonll(x) htobe64(x)
#define ntohll(x) be64toh(x)

#include <array>
#include <ostream>
#include <string>
#include <vector>
#include <cstring>

#include "Hash.h"

namespace collector {

class Address {
 public:
  enum class Family : unsigned char {
    UNKNOWN = 0,
    IPV4,
    IPV6,
  };

  static constexpr size_t kMaxLen = 16;
  static_assert(kMaxLen % sizeof(uint64_t) == 0);
  static constexpr size_t kU64MaxLen = kMaxLen / sizeof(uint64_t);

  static Address Any(Family family) { return Address(family); }

  Address() : Address(Family::UNKNOWN) {}

  explicit Address(Family family) : data_({0, 0}), family_(family) {}

  Address(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
      : Address(htonl(static_cast<uint32_t>(a) << 24 |
                      static_cast<uint32_t>(b) << 16 |
                      static_cast<uint32_t>(c) << 8 |
                      static_cast<uint32_t>(d))) {}

  Address(Family family, const std::array<uint8_t, kMaxLen>& data) : Address(family) {
    std::memcpy(data_.data(), data.data(), Length(family));
  }

  Address(Family family, const std::array<uint64_t, kU64MaxLen>& data) : data_(data), family_(family) {}

  // Constructs an IPv4 address given a uint32_t containing the IPv4 address in *network* byte order.
  explicit Address(uint32_t ipv4) : data_({htonll(static_cast<uint64_t>(ntohl(ipv4)) << 32), 0}), family_(Family::IPV4)
  {}

  // Constructs an IPv6 address from 4 network-encoded uint32_ts, in network order (high to low)
  explicit Address(const uint32_t (&ipv6)[4]) : family_(Family::IPV6) {
    static_assert(sizeof(data_) == sizeof(ipv6));
    std::memcpy(data_.data(), &ipv6[0], sizeof(data_));
  }

  // Constructs an IPv6 address from 2 network-encoded uint64_ts, in network order (high, low).
  Address(uint64_t ipv6_high, uint64_t ipv6_low) : data_({ipv6_high, ipv6_low}), family_(Family::IPV6) {}

  Family family() const { return family_; }
  size_t length() const { return Length(family_); }

  const std::array<uint64_t, kU64MaxLen>& array() const { return data_; }
  const void* data() const { return data_.data(); }

  const uint64_t* u64_data() const { return data_.data(); }

  size_t Hash() const { return HashAll(data_, family_); }

  bool operator==(const Address& other) const {
    return family_ == other.family_ && data_ == other.data_;
  }

  bool operator!=(const Address& other) const {
    return !(*this == other);
  }

  bool operator>(const Address& that) const {
    if (family_ != that.family_) {
      return family_ > that.family_;
    }

    return std::memcmp(data(), that.data(), length()) > 0;
  }

  bool IsNull() const {
    return std::all_of(data_.begin(), data_.end(), [](uint64_t v) { return v == 0; });
  }

  Address ToV6() const {
    switch (family_) {
      case Address::Family::IPV6:
        return *this;
      case Address::Family::IPV4: {
        uint64_t low = htonll(0x0000ffff00000000ULL | (ntohll(data_[0]) >> 32));
        return {0ULL, low};
      }
      default:
        return {};
    }
  }

  bool IsLocal() const {
    switch (family_) {
      case Family::IPV4:
        return (data_[0] & htonll(0xff00000000000000ULL)) == htonll(0x7f00000000000000ULL);
      case Family::IPV6:
        if (data_[0] != 0) {
          return false;
        }
        // Localhost IPv6 addresses are ::1 (first case) as well as IPv4-mapped IPv6 for 127.0.0.0/8 (second case).
        return data_[1] == htonll(1ULL) || (data_[1] & htonll(0xffffffffff000000ULL)) == htonll(0x0000ffff7f000000ULL);
      default:
        return false;
    }
  }

  bool IsPublic() const;

  static size_t Length(Family family) {
    switch (family) {
      case Family::IPV4:
        return 4;
      case Family::IPV6:
        return 16;
      default:
        return 0;
    }
  }

 private:
  friend std::ostream& operator<<(std::ostream& os, const Address& addr) {
    int af = (addr.family_ == Family::IPV4) ? AF_INET : AF_INET6;
    char addr_str[INET6_ADDRSTRLEN];
    const auto* data = addr.data();
    if (!inet_ntop(af, data, addr_str, sizeof(addr_str))) {
      return os << "<invalid address>";
    }
    return os << addr_str;
  }

  std::array<uint64_t, kU64MaxLen> data_;
  Family family_;
};

class IPNet {
 public:
  IPNet() : IPNet(Address(), 0, false) {}
  explicit IPNet(const Address& address) : IPNet(address, 8 * address.length(), true) {}
  IPNet(const Address& address, size_t bits, bool is_addr = false)
  : address_(address), mask_({0, 0}), bits_(bits), is_addr_(is_addr) {

    if (bits_ > Address::Length(address_.family()) * 8) {
      bits_ = Address::Length(address_.family()) * 8;
    }

    size_t bits_left = bits_;

    const uint64_t* in_mask_p = address.array().data();
    uint64_t* out_mask_p = mask_.data();

    while (bits_left >= 64) {
      *out_mask_p++ = *in_mask_p++;
      bits_left -= 64;
    }

    if (bits_left > 0) {
      uint64_t last_mask = ~(~static_cast<uint64_t>(0) >> bits_left);
      *out_mask_p = ntohll(*in_mask_p) & last_mask;  // last uint64 is intentionally stored in *host* order
    }
  }

  Address::Family family() const { return address_.family(); }
  const std::array<uint64_t, Address::kU64MaxLen>& mask_array() const { return mask_; }
  size_t bits() const { return bits_; }

  bool Contains(const Address& address) const {
    if (address.family() != address_.family()) {
      return false;
    }

    const uint64_t* addr_p = address.u64_data();
    const uint64_t* mask_p = mask_.data();

    size_t bitsLeft = bits_;
    while (bitsLeft >= 64) {
      if (*addr_p++ != *mask_p++) {
        return false;
      }
      bitsLeft -= 64;
    }

    if (bitsLeft > 0) {
      uint64_t lastMask = ~(~static_cast<uint64_t>(0) >> bitsLeft);
      if ((ntohll(*addr_p) & lastMask) != *mask_p) {
        return false;
      }
    }

    return true;
  }

  const Address& address() const {
    return address_;
  }

  size_t Hash() const {
    return HashAll(address_.array(), mask_, bits_);
  }

  bool IsNull() const {
    return bits_ == 0 && address_.IsNull();
  }

  bool IsAddress() const {
    return is_addr_;
  }

  bool operator==(const IPNet& other) const {
    if (bits_ != other.bits_) {
      return false;
    }
    if (is_addr_) {
      return address_ == other.address_;
    }
    return mask_ == other.mask_;
  }

  bool operator!=(const IPNet& other) const {
    return !(*this == other);
  }

  bool operator>(const IPNet& that) const {
    if (bits_ != that.bits_) {
      return bits_ > that.bits_;
    }
    return address_ > that.address_;
  }

 private:
  friend std::ostream& operator<<(std::ostream& os, const IPNet& net) {
    return os << net.address_ << "/" << net.bits_;
  }

  Address address_;
  std::array<uint64_t, Address::kU64MaxLen> mask_;
  size_t bits_;
  bool is_addr_;
};

class Endpoint {
 public:
  Endpoint() : port_(0) {}
  Endpoint(const Address& address, unsigned short port) : network_(IPNet(address)), port_(port) {}
  Endpoint(const IPNet& network, unsigned short port) : network_(network), port_(port) {}

  size_t Hash() const {
    return HashAll(network_, port_);
  }

  bool operator==(const Endpoint& other) const {
    return port_ == other.port_ && network_ == other.network_;
  }

  bool operator!=(const Endpoint& other) const {
    return !(*this == other);
  }

  const IPNet& network() const { return network_; }
  Address address() const { return network_.address(); }
  uint16_t port() const { return port_; }

  bool IsNull() const {
      return port_ == 0 && network_.IsNull();
  }

 private:
  friend std::ostream& operator<<(std::ostream& os, const Endpoint& ep) {
    // This is an individual IP address.
    if (ep.network_.bits() == 8 * ep.network_.address().length()) {
      if (ep.network_.family() == Address::Family::IPV6) {
        os << "[" << ep.network_.address() << "]";
      } else {
        os << ep.network_.address();
      }
    } else {
      // Represent network in /nn notation.
      if (ep.network_.family() == Address::Family::IPV6) {
        os << "[" << ep.network_ << "]";
      } else {
        os << ep.network_;
      }
    }
    return os << ":" << ep.port_;
  }

  IPNet network_;
  uint16_t port_;
};

enum class L4Proto : uint8_t {
  UNKNOWN = 0,
  TCP,
  UDP,
  ICMP,
};

std::ostream& operator<<(std::ostream& os, L4Proto l4proto);
using L4ProtoPortPair = ::std::pair<L4Proto, uint16_t>;

class ContainerEndpoint {
 public:
  ContainerEndpoint(std::string container, const Endpoint& endpoint, L4Proto l4proto)
      : container_(std::move(container)), endpoint_(endpoint), l4proto_(l4proto) {}

  const std::string& container() const { return container_; }
  const Endpoint& endpoint() const { return endpoint_; }
  const L4Proto l4proto() const { return l4proto_; }

  bool operator==(const ContainerEndpoint& other) const {
    return container_ == other.container_ && endpoint_ == other.endpoint_ && l4proto_ == other.l4proto_;
  }

  bool operator!=(const ContainerEndpoint& other) const {
    return !(*this == other);
  }

  size_t Hash() const { return HashAll(container_, endpoint_, l4proto_); }

 private:
  std::string container_;
  Endpoint endpoint_;
  L4Proto l4proto_;
};

std::ostream& operator<<(std::ostream& os, const ContainerEndpoint& container_endpoint);

class Connection {
 public:
  Connection() : flags_(0) {}
  Connection(std::string container, const Endpoint& local, const Endpoint& remote, L4Proto l4proto, bool is_server)
      : container_(std::move(container)), local_(local), remote_(remote), flags_((static_cast<uint8_t>(l4proto) << 1) | ((is_server) ? 1 : 0))
  {}

  const std::string& container() const { return container_; }
  const Endpoint& local() const { return local_; }
  const Endpoint& remote() const { return remote_; }
  bool is_server() const { return (flags_ & 0x1) != 0; }
  L4Proto l4proto() const { return static_cast<L4Proto>(flags_ >> 1); }

  bool operator==(const Connection& other) const {
    return container_ == other.container_ && local_ == other.local_ && remote_ == other.remote_ && flags_ == other.flags_;
  }

  bool operator!=(const Connection& other) const {
    return !(*this == other);
  }

  size_t Hash() const { return HashAll(container_, local_, remote_, flags_); }

 private:
  std::string container_;
  Endpoint local_;
  Endpoint remote_;
  uint8_t flags_;
};

std::ostream& operator<<(std::ostream& os, const Connection& conn);

// Checks if the given connection is relevant (i.e., it is a connection with a remote address that is
// not a local loopback address).
inline bool IsRelevantConnection(const Connection& conn) {
  return !conn.remote().address().IsLocal();
}

// Checks if the given endpoints is relevant (i.e., it is not only listening on local loopback).
inline bool IsRelevantEndpoint(const Endpoint& ep) {
  return !ep.address().IsLocal();
}

// IsEphemeralPort checks if the given port looks like an ephemeral (i.e., client-side) port. Note that not all
// operating systems adhere to the IANA-recommended range. Therefore, the return value is not a bool, but instead an
// int which indicates the confidence that the port is in fact ephemeral.
inline int IsEphemeralPort(uint16_t port) {
  if (port >= 49152) return 4;  // IANA range
  if (port >= 32768) return 3;  // Modern Linux kernel range
  if (port >= 1025 && port <= 5000) return 2;  // FreeBSD (partial) + Windows <=XP range
  if (port == 1024) return 1;  // FreeBSD
  return 0;  // not ephemeral according to any range
}

// PrivateIPv4Networks return private IPv4 networks.
static inline const std::vector<IPNet>& PrivateIPv4Networks() {
  static auto* networks = new std::vector<IPNet>{
    IPNet(Address(10, 0, 0, 0), 8),
    IPNet(Address(100, 64, 0, 0), 10),
    IPNet(Address(169, 254, 0, 0), 16),
    IPNet(Address(172, 16, 0, 0), 12),
    IPNet(Address(192, 168, 0, 0), 16),
    };

  return *networks;
}

// PrivateIPv6Networks returns private IPv6 networks.
static inline const std::vector<IPNet>& PrivateIPv6Networks() {
  static auto* networks = []() {
    auto* networks = new std::vector<IPNet>();
    const auto& ipv4_nets = PrivateIPv4Networks();
    networks->reserve(ipv4_nets.size() + 1);
    networks->emplace_back(Address(htonll(0xfd00000000000000ULL), 0ULL), 8);  // ULA
    for (const auto& ipv4_net : ipv4_nets) {
      networks->emplace_back(ipv4_net.address().ToV6(), ipv4_net.bits() + 96);
    }
    return networks;
  }();

  return *networks;
}

static inline const std::vector<IPNet>& PrivateNetworks(Address::Family family) {
  static std::vector<IPNet>* no_networks = new std::vector<IPNet>;
  switch (family) {
    case Address::Family::IPV4:
      return PrivateIPv4Networks();
    case Address::Family::IPV6:
      return PrivateIPv6Networks();
    default:
      return *no_networks;
  }
}


}  // namespace collector

namespace std {
size_t Hash(const collector::L4ProtoPortPair& pp);
}  // namespace std

#endif //COLLECTOR_NETWORKCONNECTION_H
