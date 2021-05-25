/* Based on src/core/ngx_radix_tree.c from NGINX copyright Igor Sysoev
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

#include "NRadix.h"
#include "Utility.h"

namespace collector {

bool NRadixTree::Insert(const IPNet& network) {
  if (network.IsNull()) {
    return false;
  }

  if (network.bits() == 0) {
    CLOG(ERROR) << "Cannot handle CIDR " << network << " with /0, in network tree";
    return false;
  }

  const uint64_t* addr_p = network.address().u64_data();
  const auto net_mask = network.net_mask_array();
  const uint64_t *net_mask_p = net_mask.data();
  uint64_t bit(0x8000000000000000ULL);

  nRadixNode* node = this->root_;
  nRadixNode* next = this->root_;

  size_t i = 0;
  // Traverse the tree for the bits that already exist in the tree.
  while (bit & *net_mask_p) {
    // If the bit is set, go right, otherwise left.
    if (ntohll(*addr_p) & bit) {
      next = node->right_;
    } else {
      next = node->left_;
    }

    if (next == nullptr) {
      break;
    }

    bit >>= 1;
    node = next;

    if (bit == 0) {
      // We have walked 128 bits, return.
      if (++i >= Address::kU64MaxLen) {
        return false;
      }

      // Reset and move to lower part.
      bit = 0x8000000000000000ULL;
      if (network.bits() >= 64) {
        *addr_p++;
        *net_mask_p++;
      }
    }
  }

  // We finished walking network bits of mask and a node already exist, try updating it with the value.
  if (next) {
    // Node already filled. Indicate that the new node was not actually inserted.
    if (!node->value_.IsNull()) {
      CLOG(ERROR) << "Failed to insert CIDR " << network << " in network tree. CIDR already exists";
      return false;
    }
    node->value_ = network;
    return true;
  }

  // There still are bits to be walked, so go ahead and add them to the tree.
  while (bit & *net_mask_p) {
    next = new nRadixNode();
    next->parent_ = node;

    if (ntohll(*addr_p) & bit) {
      node->right_ = next;
    } else {
      node->left_ = next;
    }

    bit >>= 1;
    node = next;

    if (bit == 0) {
      // We have walked all bits, stop.
      if (++i >= Address::kU64MaxLen) {
        break;
      }

      bit = 0x8000000000000000ULL;
      if (network.bits() >= 64) {
        *addr_p++;
        *net_mask_p++;
      }
    }
  }

  node->value_ = network;
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

  const uint64_t *addr_p = network.address().u64_data();
  const auto net_mask = network.net_mask_array();
  const uint64_t *net_mask_p = net_mask.data();
  uint64_t bit(0x8000000000000000ULL);

  IPNet ret;
  nRadixNode *node = this->root_;
  size_t i = 0;
  while (node) {
    if (!node->value_.IsNull()) {
      ret = node->value_;
    }

    if (ntohll(*addr_p) & bit) {
      node = node->right_;
    } else {
      node = node->left_;
    }

    // All network bits are traversed. If a supernet was found along the way, `ret` holds it,
    // else there does not exist any supernet containing the search network/address.
    if (!(*net_mask_p & bit)) {
      break;
    }

    bit >>= 1;

    if (bit == 0) {
      // We have walked 128 bits, stop.
      if (++i >= Address::kU64MaxLen) {
        break;
      }

      bit = 0x8000000000000000ULL;
      if (network.bits() >= 64) {
        *addr_p++;
        *net_mask_p++;
      }
    }
  }
  return ret;
}

IPNet NRadixTree::Find(const Address& addr) const {
  return Find(IPNet(addr));
}

void getAll(nRadixNode *node, std::vector<IPNet>& ret) {
  if (!node) {
    return;
  }

  if (!node->value_.IsNull()) {
    std::cout << node->value_;
    ret.push_back(node->value_);
  }

  getAll(node->left_, ret);
  getAll(node->right_, ret);
}

std::vector<IPNet> NRadixTree::GetAll() const {
  std::vector<IPNet> ret;
  getAll(this->root_, ret);
  return ret;
}

}
