/* NRadix.cpp - Radix tree for IP network lookup.
 *
 * Backed by a formally verified Pulse/F* implementation.
 * This file provides C++ wrappers that forward to the verified C API.
 */

#include "NRadix.h"

#include "Utility.h"

namespace collector {

namespace {

NetworkTypes_address to_verified_addr(const Address& addr) {
  NetworkTypes_address va;
  va.hi = addr.array()[0];
  va.lo = addr.array()[1];
  switch (addr.family()) {
    case Address::Family::IPV4:
      va.family = NetworkTypes_FamilyIPv4;
      break;
    case Address::Family::IPV6:
      va.family = NetworkTypes_FamilyIPv6;
      break;
    default:
      va.family = NetworkTypes_FamilyUnknown;
      break;
  }
  return va;
}

NetworkTypes_ipnet to_verified_ipnet(const IPNet& net) {
  auto va = to_verified_addr(net.address());
  return NetworkTypes_mk_ipnet(va, static_cast<uint8_t>(net.bits()));
}

Address::Family from_verified_family(NetworkTypes_address_family f) {
  switch (f) {
    case NetworkTypes_FamilyIPv4:
      return Address::Family::IPV4;
    case NetworkTypes_FamilyIPv6:
      return Address::Family::IPV6;
    default:
      return Address::Family::UNKNOWN;
  }
}

Address from_verified_addr(const NetworkTypes_address& va) {
  auto family = from_verified_family(va.family);
  std::array<uint64_t, Address::kU64MaxLen> data = {va.hi, va.lo};
  return Address(family, data);
}

IPNet from_verified_ipnet(const NetworkTypes_ipnet& vn) {
  auto addr = from_verified_addr(vn.addr);
  return IPNet(addr, vn.prefix);
}

void collectAll(const FStar_Pervasives_Native_option___NRadixPulse_node_t_& ct,
                std::vector<IPNet>& result) {
  if (ct.tag == FStar_Pervasives_Native_None) return;
  NRadixPulse_node_t* node = ct.v;
  if (node->has_value) {
    result.push_back(from_verified_ipnet(node->value));
  }
  collectAll(node->left, result);
  collectAll(node->right, result);
}

}  // namespace

NRadixTree::NRadixTree(const NRadixTree& other) : tree_(NRadixPulse_create()) {
  auto nets = other.GetAll();
  for (const auto& net : nets) {
    tree_ = NRadixPulse_insert(tree_, to_verified_ipnet(net));
  }
}

NRadixTree& NRadixTree::operator=(const NRadixTree& other) {
  if (this == &other) {
    return *this;
  }
  NRadixPulse_destroy(tree_);
  tree_ = NRadixPulse_create();
  auto nets = other.GetAll();
  for (const auto& net : nets) {
    tree_ = NRadixPulse_insert(tree_, to_verified_ipnet(net));
  }
  return *this;
}

bool NRadixTree::Insert(const IPNet& network) {
  if (network.IsNull()) {
    CLOG(ERROR) << "Cannot handle null IP networks in network tree";
    return false;
  }

  if (network.bits() < 1 || network.bits() > 128) {
    CLOG(ERROR) << "Cannot handle CIDR " << network << " with /" << network.bits() << " , in network tree";
    return false;
  }

  tree_ = NRadixPulse_insert(tree_, to_verified_ipnet(network));
  return true;
}

IPNet NRadixTree::Find(const IPNet& network) const {
  if (network.IsNull()) {
    return {};
  }

  if (network.bits() == 0) {
    CLOG(ERROR) << "Cannot handle CIDR " << network << " with /0, in network tree";
    return {};
  }

  auto vn = to_verified_ipnet(network);
  auto result = NRadixPulse_find(tree_, vn);
  if (!result.found) {
    return {};
  }

  auto ret = from_verified_ipnet(result.net);
  return (network.family() == ret.family()) ? ret : IPNet();
}

IPNet NRadixTree::Find(const Address& addr) const {
  if (addr.family() == Address::Family::UNKNOWN) {
    return {};
  }

  auto va = to_verified_addr(addr);
  auto result = NRadixPulse_find_addr(tree_, va);
  if (!result.found) {
    return {};
  }

  auto ret = from_verified_ipnet(result.net);
  auto ret_family = from_verified_family(result.net.addr.family);
  return (addr.family() == ret_family) ? ret : IPNet();
}

std::vector<IPNet> NRadixTree::GetAll() const {
  std::vector<IPNet> ret;
  collectAll(tree_, ret);
  return ret;
}

bool NRadixTree::IsEmpty() const {
  return NRadixPulse_is_empty(tree_);
}

bool NRadixTree::IsAnyIPNetSubset(const NRadixTree& other) const {
  return this->IsAnyIPNetSubset(Address::Family::UNKNOWN, other);
}

bool NRadixTree::IsAnyIPNetSubset(Address::Family family, const NRadixTree& other) const {
  auto otherNets = other.GetAll();
  for (const auto& net : otherNets) {
    if (family != Address::Family::UNKNOWN && net.family() != family) continue;
    auto found = this->Find(net.address());
    if (!found.IsNull() && found.family() == net.family()) return true;
  }
  return false;
}

}  // namespace collector
