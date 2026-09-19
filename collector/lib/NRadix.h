/* NRadix.h - Radix tree for IP network lookup.
 *
 * Backed by a formally verified Pulse/F* implementation.
 * The verified C library provides insert, find, and related operations
 * with proven correctness guarantees.
 */

#pragma once

#include <mutex>
#include <vector>

#include "Logging.h"
#include "NetworkConnection.h"
#include "Utility.h"

extern "C" {
#include "extracted/NRadixPulse.h"
#include "extracted/internal/NetworkTypes.h"
}

namespace collector {

class NRadixTree {
 public:
  NRadixTree() : tree_(NRadixPulse_create()) {}
  explicit NRadixTree(const std::vector<IPNet>& networks) : tree_(NRadixPulse_create()) {
    for (const auto& network : networks) {
      auto inserted = this->Insert(network);
      if (!inserted) {
        CLOG(ERROR) << "Failed to insert CIDR " << network << " in network tree";
      }
    }
  }

  NRadixTree(const NRadixTree& other);

  ~NRadixTree() {
    NRadixPulse_destroy(tree_);
  }

  NRadixTree& operator=(const NRadixTree& other);

  // Inserts a network into radix tree. If the network already exists, insertion is skipped.
  // This function does not guarantee thread safety.
  bool Insert(const IPNet& network);
  // Returns the smallest subnet larger than or equal to the queried network.
  // This function does not guarantee thread safety.
  IPNet Find(const IPNet& network) const;
  // Returns the smallest subnet larger than or equal to the queried address.
  // This function does not guarantee thread safety.
  IPNet Find(const Address& addr) const;
  // Returns a vector of all the stored networks.
  std::vector<IPNet> GetAll() const;
  // Tells whether the RadixTree contains no network.
  bool IsEmpty() const;
  // Determines whether any network in `other` is fully contained by any network in this tree.
  bool IsAnyIPNetSubset(const NRadixTree& other) const;
  // Determines whether any network in `other` is fully contained by any network in this tree, for a given family.
  bool IsAnyIPNetSubset(Address::Family family, const NRadixTree& other) const;

 private:
  mutable NRadixPulse_nradix_tree tree_;
};

}  // namespace collector
