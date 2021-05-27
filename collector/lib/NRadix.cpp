/* Based on src/core/ngx_radix_tree.c from NGINX
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

#include "NRadix.h"
#include "Utility.h"

namespace collector {

bool NRadixTree::Insert(const IPNet& network) const {
  if (network.IsNull()) {
    CLOG(ERROR) << "Cannot handle null IP networks in network tree";
    return false;
  }

  if (network.bits() < 1 || network.bits() > 128) {
    CLOG(ERROR) << "Cannot handle CIDR " << network << " with /" << network.bits() << " , in network tree";
    return false;
  }

  const uint64_t* addr_p = network.address().u64_data();
  const auto net_mask = network.net_mask_array();
  const uint64_t* net_mask_p = net_mask.data();
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

    if (!next) break;

    bit >>= 1;
    node = next;

    if (bit == 0) {
      // We have walked 128 bits, stop.
      if (++i >= Address::kU64MaxLen) break;

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
    if (node->value_) {
      CLOG(ERROR) << "CIDR " << network << " already exists";
      return false;
    }
    node->value_ = new IPNet(network);
    return true;
  }

  // There still are bits to be walked, so go ahead and add them to the tree.
  while (bit & *net_mask_p) {
    next = new nRadixNode();

    if (ntohll(*addr_p) & bit) {
      node->right_ = next;
    } else {
      node->left_ = next;
    }

    bit >>= 1;
    node = next;

    if (bit == 0) {
      // We have walked all 128 bits, stop.
      if (++i >= Address::kU64MaxLen) break;

      bit = 0x8000000000000000ULL;
      if (network.bits() >= 64) {
        *addr_p++;
        *net_mask_p++;
      }
    }
  }

  node->value_ = new IPNet(network);
  return true;
}

IPNet NRadixTree::Find(const IPNet& network) const {
  if (network.IsNull()) return {};

  if (network.bits() == 0) {
    CLOG(ERROR) << "Cannot handle CIDR " << network << " with /0, in network tree";
    return {};
  }

  const uint64_t *addr_p = network.address().u64_data();
  const auto net_mask = network.net_mask_array();
  const uint64_t *net_mask_p = net_mask.data();
  uint64_t bit(0x8000000000000000ULL);

  IPNet ret;
  nRadixNode* node = this->root_;
  size_t i = 0;
  while (node) {
    if (node->value_) {
      ret = *node->value_;
    }

    if (ntohll(*addr_p) & bit) {
      node = node->right_;
    } else {
      node = node->left_;
    }

    // All network bits are traversed. If a supernet was found along the way, `ret` holds it,
    // else there does not exist any supernet containing the search network/address.
    if (!(*net_mask_p & bit)) break;

    bit >>= 1;

    if (bit == 0) {
      // We have walked 128 bits, stop.
      if (++i >= Address::kU64MaxLen) {
        if (node->value_) {
          ret = *node->value_;
        }
        break;
      }

      bit = 0x8000000000000000ULL;
      if (network.bits() >= 64) {
        *addr_p++;
        *net_mask_p++;
      }
    }
  }

  return (network.family() == ret.family()) ? ret : IPNet();
}

IPNet NRadixTree::Find(const Address& addr) const {
  return Find(IPNet(addr));
}

void getAll(nRadixNode* node, std::vector<IPNet>& ret) {
  if (!node) return;

  if (node->value_) {
    ret.push_back(*node->value_);
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
