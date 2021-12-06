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

#ifndef COLLECTOR_STRINGVIEW_H
#define COLLECTOR_STRINGVIEW_H

#include <cstring>
#include <ostream>
#include <string>
#include <vector>

namespace collector {
class StringView {
 public:
  using size_type = std::string::size_type;
  static constexpr size_type npos = std::string::npos;

  using value_type = char;
  using const_reference = const char&;
  using const_iterator = const char*;
  using const_pointer = const char*;

  StringView() : p_(nullptr), n_(0) {}
  StringView(const char* p) : p_(p), n_(std::strlen(p)) {}
  StringView(const char* p, size_type n) : p_(p), n_(n) {}
  StringView(const StringView& other) : p_(other.p_), n_(other.n_) {}
  template <std::size_t N>
  StringView(const char (&buf)[N]) : p_(&buf[0]), n_(N - 1) {}

  operator bool() const { return n_ > 0; }
  std::string str() const { return std::string(p_, n_); }

  size_type size() const { return n_; }

  char operator[](int idx) const { return p_[idx]; }

  size_type find(char c, size_type pos = 0) const {
    if (pos >= n_) return npos;
    const char* occ = static_cast<const char*>(std::memchr(p_ + pos, c, n_ - pos));
    return occ ? occ - p_ : npos;
  }

  StringView substr(size_type pos = 0, size_type count = npos) const {
    if (pos >= n_) return {};
    const char* new_p = p_ + pos;
    size_type new_n = n_ - pos;
    if (count == npos) return {new_p, new_n};
    if (count < 0) return {};
    if (new_n > count) new_n = count;
    return {new_p, new_n};
  }

  // Splits this view into string copies of parts of the data, delimited
  // by the given character.
  std::vector<std::string> split(char delim = ' ') const {
    std::vector<std::string> parts;
    size_type offset = 0;

    for (size_type n = find(delim); n != npos; n = find(delim, offset)) {
      parts.push_back(substr(offset, n - offset).str());
      offset = n + 1;
    }

    // Push remainder of the string. This may be empty if the string
    // ends with a delimiter.
    parts.push_back(substr(offset).str());

    return parts;
  }

  void remove_prefix(size_type n) {
    if (n > n_) n = n_;
    p_ += n;
    n_ -= n;
  }

  void remove_suffix(size_type n) {
    if (n > n_) n = n_;
    n_ -= n;
  }

  const_iterator begin() const { return p_; }
  const_iterator end() const { return p_ + n_; }

  friend std::ostream& operator<<(std::ostream& out, StringView const& sv) {
    out.write(sv.p_, sv.n_);
    return out;
  }

  template <std::size_t N>
  bool operator==(const char (&str)[N]) const {
    if (n_ != N - 1) return false;
    if (N - 1 == 0) return true;
    return std::memcmp(p_, &str[0], N - 1) == 0;
  }

 private:
  const char* p_;
  size_type n_;
};
}  // namespace collector

#endif  // COLLECTOR_STRINGVIEW_H
