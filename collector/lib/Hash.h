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

#ifndef COLLECTOR_HASH_H
#define COLLECTOR_HASH_H

#include <algorithm>
#include <unordered_map>

namespace collector {

namespace internal {

template <typename T>
class HasHash {
 private:
  typedef char YesType[1];
  typedef char NoType[2];

  template <typename C> static YesType& test(typename std::enable_if<std::is_same<decltype(std::declval<const C>().Hash()), size_t>::value>::type*) ;
  template <typename C> static NoType& test(...);

 public:
  enum { value = sizeof(test<T>(0)) == sizeof(YesType) };
};

}  // namespace internal

struct Hash {
  template <typename T>
  typename std::enable_if<internal::HasHash<T>::value, size_t>::type operator() (const T& val) const {
    return val.Hash();
  }

  template <typename T>
  typename std::enable_if<!internal::HasHash<T>::value, size_t>::type operator() (const T& val) const {
    return std::hash<T>()(val);
  }
};

template <typename Arg>
size_t CombineHashes(const Arg& arg) {
  return Hash()(arg);
}

template <typename FirstArg, typename... RestArgs>
size_t CombineHashes(const FirstArg& first, const RestArgs&... rest) {
  size_t rest_hash = CombineHashes(rest...);
  return rest_hash ^ (Hash()(first) + 0x9e3779b9 + (rest_hash<<6) + (rest_hash>>2));
}

template <typename K, typename V>
using UnorderedMap = std::unordered_map<K, V, Hash>;

}  // namespace collector

#endif //COLLECTOR_HASH_H
