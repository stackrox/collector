#ifndef _STRING_UTIL_H_
#define _STRING_UTIL_H_

#include <sstream>

namespace collector {

namespace internal {

void DoStrCat(std::ostringstream* os) {}

template <typename Arg, typename... Args>
void DoStrCat(std::ostringstream* os, const Arg& arg, const Args&... rest) {
    *os << arg;
    DoStrCat(os, rest...);
}

}  // namespace internal

template<typename... Args>
std::string StrCat(const Args&... args) {
    std::ostringstream os;
    internal::DoStrCat(&os, args...);
    return os.str();
}

}  // namespace collector

#endif
