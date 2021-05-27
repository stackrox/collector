/* Based on src/core/ngx_radix_tree.h from NGINX copyright Igor Sysoev
 *
 * Additional changes are licensed under the same terms as NGINX and
 * copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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

  std::vector<IPNet> GetAll() const;

  nRadixNode* root_;
};

} // namespace collector

#endif //COLLECTOR_NRADIX_H
