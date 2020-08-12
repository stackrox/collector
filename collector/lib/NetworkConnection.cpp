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

#include "NetworkConnection.h"

#include <vector>

namespace collector {

namespace {

static const std::vector<IPNet>& PrivateIPv4Networks() {
  static auto* networks = new std::vector<IPNet>{
    IPNet(Address(10, 0, 0, 0), 8),
    IPNet(Address(100, 64, 0, 0), 10),
    IPNet(Address(169, 254, 0, 0), 16),
    IPNet(Address(172, 16, 0, 0), 12),
    IPNet(Address(192, 168, 0, 0), 16),
  };

  return *networks;
}

static const std::vector<IPNet>& PrivateIPv6Networks() {
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

}  // namespace

bool Address::IsPublic() const {
  switch (family_) {
    case Family::IPV4:
      for (const auto& net : PrivateIPv4Networks()) {
        if (net.Contains(*this)) {
          return false;
        }
      }
      return true;

    case Family::IPV6:
      for (const auto& net : PrivateIPv6Networks()) {
        if (net.Contains(*this)) {
          return false;
        }
      }
      return true;

    default:
      return false;
  }
}

std::ostream& operator<<(std::ostream& os, L4Proto l4proto) {
  switch (l4proto) {
    case L4Proto::TCP:
      return os << "tcp";
    case L4Proto::UDP:
      return os << "udp";
    case L4Proto::ICMP:
      return os << "icmp";
    default:
      return os << "unknown(" << static_cast<uint8_t>(l4proto) << ")";
  }
}

std::ostream& operator<<(std::ostream& os, const ContainerEndpoint& container_endpoint) {
  return os << container_endpoint.container() << ": " << container_endpoint.endpoint();
}

std::ostream& operator<<(std::ostream& os, const Connection& conn) {
  os << conn.container() << ": " << conn.local();
  if (conn.is_server()) {
    os << " <- ";
  } else {
    os << " -> ";
  }
  os << conn.remote() << " [" << conn.l4proto();
  if (conn.local().address().family() == Address::Family::IPV6) {
    os << "6";
  }
  return os << "]";
}

}  // namespace collector
