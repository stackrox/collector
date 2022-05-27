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
#include <unordered_set>

namespace collector {

namespace internal {

// SFINAE helper to check if a type has a `size_t Hash() const` method.
template <typename T>
class HasHash {
 private:
  typedef char YesType[1];
  typedef char NoType[2];

  template <typename C>
  static YesType& test(typename std::enable_if<std::is_same<decltype(std::declval<const C>().Hash()), size_t>::value>::type*);
  template <typename C>
  static NoType& test(...);

 public:
  enum { value = sizeof(test<T>(0)) == sizeof(YesType) };
};

template <typename T>
class HasStdHashSpecialization {
 private:
  typedef char YesType[1];
  typedef char NoType[2];

  template <typename C>
  static YesType& test(std::integral_constant<size_t, sizeof(std::hash<C>)>*);
  template <typename C>
  static NoType& test(...);

 public:
  enum { value = sizeof(test<T>(0)) == sizeof(YesType) };
};

}  // namespace internal

// Hash specialization for types that define a `Hash()` method.
template <typename T>
size_t Hash(const T& val, typename std::enable_if<internal::HasHash<T>::value>::type* = 0) {
  return val.Hash();
}

// Hash specialization for types that have a std::hash<T> specialization.
template <typename T>
auto Hash(const T& val, typename std::enable_if<!internal::HasHash<T>::value>::type* = 0) -> decltype(std::hash<T>()(val)) {
  return std::hash<T>()(val);
}

// Hash specialization for enums (defer to underlying type hash).
template <typename T>
size_t Hash(const T& val, typename std::enable_if<std::is_enum<T>::value && !internal::HasStdHashSpecialization<T>::value>::type* = 0) {
  return Hash(static_cast<typename std::underlying_type<T>::type>(val));
}

// CombineHashes combines two hashes.
inline size_t CombineHashes(size_t seed, size_t hash) {
  return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

// Hash specialization for arrays.
template <typename T, size_t N>
size_t Hash(const std::array<T, N>& array) {
  size_t hash = Hash(array[0]);
  for (size_t i = 1; i < N; i++) {
    hash = CombineHashes(hash, Hash(array[i]));
  }
  return hash;
}

// Hasher is a function object that hashes values by calling the free function Hash(value), in an ADL-enabled fashion.
struct Hasher {
  template <typename T>
  size_t operator()(const T& val) const {
    return Hash(val);
  }
};

// HashAll can be used to combine the hashes of several objects.
template <typename Arg>
size_t HashAll(const Arg& arg) {
  return Hasher()(arg);
}

template <typename FirstArg, typename... RestArgs>
size_t HashAll(const FirstArg& first, const RestArgs&... rest) {
  return CombineHashes(Hasher()(first), HashAll(rest...));
}

template <typename E>
using UnorderedSet = std::unordered_set<E, Hasher>;

template <typename K, typename V>
using UnorderedMap = std::unordered_map<K, V, Hasher>;

}  // namespace collector

#endif  // COLLECTOR_HASH_H
