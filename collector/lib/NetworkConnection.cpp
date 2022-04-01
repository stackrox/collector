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

size_t Hash(const L4ProtoPortPair& pp) {
  return HashAll(pp.first, pp.second);
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
