#include "NetworkConnection.h"

#include <netdb.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Process.h"

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
  if (container_endpoint.originator()) {
    return os << container_endpoint.container() << ": " << container_endpoint.endpoint() << ": " << *container_endpoint.originator();
  } else {
    return os << container_endpoint.container() << ": " << container_endpoint.endpoint();
  }
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

static bool parse_address(const char* address, struct sockaddr_storage& sockaddr) {
  struct addrinfo hints = {.ai_flags = AI_NUMERICHOST, 0};
  struct addrinfo* addrinfo = NULL;
  bool success;

  success = 0 == getaddrinfo(address, NULL, &hints, &addrinfo);

  if (addrinfo && addrinfo->ai_addr) {
    memcpy(&sockaddr, reinterpret_cast<struct sockaddr_storage*>(addrinfo->ai_addr), addrinfo->ai_addrlen);
  } else {
    success = false;
  }

  freeaddrinfo(addrinfo);

  return success;
}

static Address::Family get_family(const struct sockaddr_storage& sockaddr) {
  switch (sockaddr.ss_family) {
    case AF_INET:
      return Address::Family::IPV4;
    case AF_INET6:
      return Address::Family::IPV6;
    default:
      return Address::Family::UNKNOWN;
  }
}

static const uint8_t* get_raw_address(const struct sockaddr_storage& sockaddr) {
  switch (sockaddr.ss_family) {
    case AF_INET:
      return reinterpret_cast<const uint8_t*>(&reinterpret_cast<const struct sockaddr_in&>(sockaddr).sin_addr.s_addr);
    case AF_INET6:
      return reinterpret_cast<const uint8_t*>(reinterpret_cast<const struct sockaddr_in6&>(sockaddr).sin6_addr.s6_addr);
    default:
      return NULL;
  }
}

std::optional<Address> Address::parse(const std::string& address_string) {
  struct sockaddr_storage sockaddr;

  if (parse_address(address_string.c_str(), sockaddr)) {
    const uint8_t* raw_address = get_raw_address(sockaddr);
    if (raw_address == NULL) {
      return std::nullopt;
    }
    return Address(get_family(sockaddr), reinterpret_cast<const std::array<uint8_t, kMaxLen>&>(*raw_address));
  }

  return std::nullopt;
}

}  // namespace collector
