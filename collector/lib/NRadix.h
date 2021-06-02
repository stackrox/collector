/* Based on src/core/ngx_radix_tree.h from NGINX
 *
 * Copyright (C) 2002-2021 Igor Sysoev
 * Copyright (C) 2011-2021 Nginx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef COLLECTOR_NRADIX_H
#define COLLECTOR_NRADIX_H

#include <mutex>

#include "Logging.h"
#include "NetworkConnection.h"
#include "Utility.h"

namespace collector {

struct nRadixNode {
  nRadixNode() : value_(nullptr), left_(nullptr), right_(nullptr) {}
  explicit nRadixNode(const IPNet& value) : value_(new IPNet(value)), left_(nullptr), right_(nullptr) {}

  nRadixNode(const nRadixNode& other): value_(nullptr), left_(nullptr), right_(nullptr) {
    if (other.value_) {
      value_ = new IPNet(*other.value_);
    }
    if (other.left_) {
      left_ = new nRadixNode(*other.left_);
    }
    if (other.right_) {
      right_ = new nRadixNode(*other.right_);
    }
  }

  nRadixNode& operator=(const nRadixNode& other) {
    if (this == &other) return *this;
    auto* new_node = new nRadixNode(other);
    std::swap(*new_node, *this);
    delete new_node;
    return *this;
  }

  ~nRadixNode() {
    delete left_;
    delete right_;
    delete value_;
  }

  const IPNet* value_;
  nRadixNode* left_;
  nRadixNode* right_;
};

class NRadixTree {
 public:
  NRadixTree(): root_(new nRadixNode()) {}
  explicit NRadixTree(const std::vector<IPNet>& networks): root_(new nRadixNode()) {
    for (const auto& network : networks) {
      auto inserted = this->Insert(network);
      if (!inserted) {
        CLOG(ERROR) << "Failed to insert CIDR " << network << " in network tree";
        delete root_;
      }
    }
  }

  NRadixTree(const NRadixTree& other): root_(new nRadixNode(*other.root_)) {}

  ~NRadixTree() {
    // This calls the node destructor which in turn cleans up all the nodes.
    delete root_;
  }

  NRadixTree& operator=(const NRadixTree& other)  {
    if (this == &other) return *this;
    delete root_;
    // This calls the node copy constructor which in turn copies all the nodes.
    root_ = new nRadixNode(*other.root_);
    return *this;
  }

  // Inserts a network into radix tree. If the network already exists, insertion is skipped.
  // This function does not guarantee thread safety.
  bool Insert(const IPNet& network) const;
  // Returns the smallest subnet larger than or equal to the queried network.
  // This function does not guarantee thread safety.
  IPNet Find(const IPNet& network) const;
  // Returns the smallest subnet larger than or equal to the queried address.
  // This function does not guarantee thread safety.
  IPNet Find(const Address& addr) const;
  // Returns a vector of all the stored networks.
  std::vector<IPNet> GetAll() const;
  // Determines whether any network in `other` is fully contained by any network in this tree.
  bool IsAnyIPNetSubset(const NRadixTree& other) const;
  // Determines whether any network in `other` is fully contained by any network in this tree, for a given family.
  bool IsAnyIPNetSubset(Address::Family family, const NRadixTree& other) const;

  nRadixNode* root_;
};

} // namespace collector

#endif //COLLECTOR_NRADIX_H
