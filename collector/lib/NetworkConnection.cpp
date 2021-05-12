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

namespace collector {

bool Address::IsPublic() const {
  for (const auto& net : PrivateNetworks(family_)) {
    if (net.Contains(*this)) {
      return false;
    }
  }
  return true;
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

namespace std {
size_t Hash(const collector::L4ProtoPortPair& pp) {
  return HashAll(pp.first, pp.second);
}
}  // namespace std
