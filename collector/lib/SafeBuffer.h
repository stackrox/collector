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

#ifndef _SAFE_BUFFER_H_
#define _SAFE_BUFFER_H_

extern "C" {
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
}

#include <stdexcept>
#include <string>

// A buffer with a fixed maximum size that allows for safely appending strings to it.
class SafeBuffer {
 public:
  static const int DEFAULT_SIZE = 4096;

  SafeBuffer(char* buffer, int bufferSize, bool takeOwnership)
      : m_buffer(buffer), m_owns(takeOwnership) {
    if (bufferSize <= 0) {
      throw std::invalid_argument("SafeBuffer size cannot be negative");
    }
    m_endp = m_buffer + bufferSize;
    clear();
  }

  SafeBuffer(char* buffer, int bufferSize) : SafeBuffer(buffer, bufferSize, false) {}
  SafeBuffer(int bufferSize)
    : SafeBuffer(new char[bufferSize <= 0 ? DEFAULT_SIZE : bufferSize], bufferSize <= 0 ? DEFAULT_SIZE : bufferSize, true) {}
  SafeBuffer() : SafeBuffer(DEFAULT_SIZE) {}
  ~SafeBuffer() {
    if (m_owns) {
      delete[] m_buffer;
    }
  }

  void reset(char* buffer, int bufferSize, bool takeOwnership = false) {
    if (bufferSize <= 0) {
      throw std::invalid_argument("SafeBuffer size cannot be negative");
    }
    if (m_owns) {
      delete[] m_buffer;
    }
    m_buffer = buffer;
    m_endp = buffer + bufferSize;
    m_owns = takeOwnership;
  }

  void reset(int bufferSize) {
    if (bufferSize <= 0) {
      bufferSize = DEFAULT_SIZE;
    }
    reset(new char[bufferSize], bufferSize, true);
  }

  // Clears the content of the buffer.
  void clear() {
    m_currp = m_buffer;
    *m_currp = '\0';
  }

  char& operator[](int i) {
    char* p = m_buffer + i;
    if (i < 0 || p >= m_endp) {
      throw std::out_of_range("invalid SafeBuffer subscript");
    }
    return *p;
  }

  char operator[](int i) const {
    const char* p = m_buffer + i;
    if (i < 0 || p >= m_endp) {
      throw std::out_of_range("invalid SafeBuffer subscript");
    }
    return *p;
  }

  // Obtains a pointer to the raw buffer.
  char* buffer() {
    return m_buffer;
  }
  const char* buffer() const {
    return m_buffer;
  }

  bool empty() const {
    return m_currp == m_buffer;
  }

  int size() const {
    return m_currp - m_buffer;
  }

  int remaining() const {
    return m_endp - m_currp;
  }

  void Truncate(int n) {
    if (n > 0) {
      if (n >= m_currp - m_buffer) {
        return;
      }
      m_currp = m_buffer + n;
    } else if (n < 0) {
      n = -n;
      if (n >= m_currp - m_buffer) {
        m_currp = m_buffer;
      } else {
        m_currp -= n;
      }
    }
    *m_currp = '\0';
  }

  // If there is enough space left, appends the given single character to the buffer and returns true. Otherwise,
  // nothing happens and false is returned.
  bool Append(char c) {
    if (m_currp + 1 >= m_endp) {
      return false;
    }
    *m_currp = c;
    *++m_currp = '\0';
    return true;
  }

  // Appends the given printf-style formatted string to the buffer, truncating it if necessary. The result indicates
  // whether truncation happened.
  bool AppendFTrunc(const char* fmt, ...) {
    int nRemaining = remaining();
    va_list ap;
    va_start(ap, fmt);
    int written = vsnprintf(m_currp, nRemaining, fmt, ap);
    va_end(ap);
    if (written >= nRemaining) {
      m_currp += nRemaining - 1;
      return false;
    }
    m_currp += written;
    return true;
  }

  bool AppendTrunc(const std::string& str) {
    return AppendTrunc(str.data(), str.length());
  }

  bool AppendTrunc(const void* data, int length) {
    int nRemaining = remaining();
    bool notrunc = true;
    if (nRemaining < length) {
      length = nRemaining;
      notrunc = false;
    }
    memcpy(m_currp, data, length);
    m_currp += length;
    *m_currp = '\0';
    return notrunc;
  }

  bool AppendWhole(const std::string& str) {
    return AppendWhole(str.data(), str.length());
  }

  bool AppendWhole(const void* data, int length) {
    if (remaining() < length) return false;
    memcpy(m_currp, data, length);
    m_currp += length;
    *m_currp = '\0';
    return true;
  }

  // If there is enough space left, appends the given printf-style formatted string to the buffer and returns true.
  // Otherwise, the contents of the buffer are left unchanged, and false is returned.
  bool AppendFWhole(const char* fmt, ...) {
    int nRemaining = remaining();
    if (nRemaining <= 1) {
      return false;
    }
    va_list ap;
    va_start(ap, fmt);
    int written = vsnprintf(m_currp, nRemaining, fmt, ap);
    va_end(ap);
    if (written >= nRemaining) {
      // Re-terminate string at current, do not advance.
      *m_currp = '\0';
      return false;
    }
    m_currp += written;
    return true;
  }

  // Returns the contents of the buffer as an std::string.
  std::string str() const {
    return std::string(m_buffer, size());
  }

 private:
  char* m_buffer;
  char* m_currp;
  char* m_endp;
  bool m_owns;
};

#endif // _SAFE_BUFFER_H_
