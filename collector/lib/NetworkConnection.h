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

  Address(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
      : Address(htonl(static_cast<uint32_t>(a) << 24 |
                      static_cast<uint32_t>(b) << 16 |
                      static_cast<uint32_t>(c) << 8 |
                      static_cast<uint32_t>(d))) {}

  Address(Family family, const std::array<uint8_t, kMaxLen>& data) : Address(family) {
    std::memcpy(data_.data(), data.data(), Length(family));
  }

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

  explicit Address(Family family) : data_({0, 0}), family_(family) {}

  std::array<uint64_t, kU64MaxLen> data_;
  Family family_;
};

class Endpoint {
 public:
  Endpoint() : port_(0) {}
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

  const Address& address() const { return address_; }
  uint16_t port() const { return port_; }

  bool IsNull() const { return port_ == 0 && address_.IsNull(); }

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


class IPNet {
 public:
  IPNet(const Address& address, size_t bits) : IPNet(address.family(), address.array(), bits) {}

  Address::Family family() const { return family_; }
  const std::array<uint64_t, Address::kU64MaxLen>& mask_array() const { return mask_; }
  size_t bits() const { return bits_; }

  bool Contains(const Address& address) const {
    if (address.family() != family_) {
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

 private:
  IPNet(Address::Family family, const std::array<uint64_t, Address::kU64MaxLen>& mask, size_t bits)
      : family_(family), mask_({0, 0}), bits_(bits) {

    if (bits_ > Address::Length(family) * 8) {
      bits_ = Address::Length(family) * 8;
    }

    size_t bits_left = bits_;

    const uint64_t* in_mask_p = mask.data();
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

  Address::Family family_;
  std::array<uint64_t, Address::kU64MaxLen> mask_;
  size_t bits_;
};

enum class L4Proto : uint8_t {
  UNKNOWN = 0,
  TCP,
  UDP,
  ICMP,
};

std::ostream& operator<<(std::ostream& os, L4Proto l4proto);

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

}  // namespace collector

#endif //COLLECTOR_NETWORKCONNECTION_H
