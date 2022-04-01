#ifndef COLLECTOR_CONTAINERS_H
#define COLLECTOR_CONTAINERS_H

// Utility methods for working with STL containers.

#include <type_traits>

namespace collector {

// WithConst expands to a version of T with an added const-qualified if and only if U is const.
template <typename T, typename U>
using WithConst = typename std::conditional<std::is_const<U>::value, const T, T>::type;

// ValueType and MappedType correspond to the value_type and mapped_type type members of STL containers, with the
// exception that they are const-qualified if the container type is const.
template <typename C>
using ValueType = WithConst<typename C::value_type, C>;
template <typename M>
using MappedType = WithConst<typename M::mapped_type, M>;

// Find returns a pointer to the element with the given key, or null if the element was not found.
template <typename C>
ValueType<C>* Find(C& container, const typename C::key_type& key) {
  auto it = container.find(key);
  if (it == container.end()) return nullptr;
  return &*it;
}

// Contains checks if the given container contains the given key.
template <typename C>
bool Contains(const C& container, const typename C::key_type& key) {
  return Find(container, key) != nullptr;
}

// Lookup retrieves a pointer to the map value corresponding to the given key, or null if the map has no entry for the
// key.
template <typename M>
MappedType<M>* Lookup(M& map, const typename M::key_type& key) {
  auto* val = Find(map, key);
  if (!val) return nullptr;
  return &val->second;
}

}  // namespace collector

#endif  //COLLECTOR_CONTAINERS_H
