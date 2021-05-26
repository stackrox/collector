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
  const IPNet* value_;
  nRadixNode* left_;
  nRadixNode* right_;
};

class NRadixTree {
 public:
  NRadixTree(): root_(new nRadixNode()) {}
  explicit NRadixTree(const std::vector<IPNet>& networks): root_(new nRadixNode()) {
    for (const auto& network : networks) {
      this->Insert(network);
    }
  }

  NRadixTree& operator=(NRadixTree other)  {
    deleteSubtree(root_);
    root_ = nullptr;
    std::swap(root_, other.root_);
    return *this;
  }

  // Inserts a network into radix tree. If the network already exists, insertion is skipped.
  // This function does not guarantee thread safety.
  bool Insert(const IPNet& network);
  // Returns the smallest subnet larger than or equal to the queried network.
  // This function does not guarantee thread safety.
  IPNet Find(const IPNet& network) const;
  // Returns the smallest subnet larger than or equal to the queried address.
  // This function does not guarantee thread safety.
  IPNet Find(const Address& addr) const;

  std::vector<IPNet> GetAll() const;

 private:
  void deleteSubtree(nRadixNode* node);

  nRadixNode* root_;
};

} // namespace collector

#endif //COLLECTOR_NRADIX_H
