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

#include "AbortHandler.h"

namespace {
void WriteBuffer(char* buffer, int num_bytes, int buffer_size) {
  if (num_bytes > buffer_size) {
    // the output was truncated, to keep it readable add a new line
    buffer[buffer_size - 1] = '\n';
    write(STDERR_FILENO, buffer, buffer_size);
  } else {
    write(STDERR_FILENO, buffer, num_bytes);
  }
}
}  // namespace

extern "C" void AbortHandler(int signum) {
  // Write a stacktrace to stderr. Since the abort handler could be called
  // after SIGSEGV or some other emergency situations:
  // * we have to be modest and use little memory (in case if the original
  //   reason was OOM)
  // * as the original implementation noted, use reentrant functions
  //
  // The latter one was refering to snprintf, although it's not clear if it is
  // reentrant. Libc docs says functions using I/O streams are potentially
  // non-reentrant [1], and it seems under the hood snprintf uses streams.
  //
  // XXX: There are portability issues with signal() [2], in the future it
  // would be better to use sigaction.
  //
  // [1]: https://www.gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Nonreentrancy.html
  // [2]: https://man7.org/linux/man-pages/man2/signal.2.html

  void* buffer[32];  // Addresses buffer

  char message_buffer[256];  // Actual text buffer for a single stack trace
                             // address. Some templated signatures could be
                             // particularly long.
  int num_bytes;

  size_t n_frames = backtrace(buffer, 32);
  size_t max_frames = sizeof(buffer) / sizeof(buffer[0]);
  int message_buffer_size = sizeof(message_buffer);

  for (size_t i = 0; i < n_frames; i++) {
    Dl_info info;

    if (dladdr(buffer[i], &info)) {
      // Try to demangle the stacktrace
      char* demangled = NULL;
      int status;
      size_t shift_from_symbol;

      // NULL as a second argument for the output buffer will make it allocate
      // new region of memory
      demangled = abi::__cxa_demangle(info.dli_sname, NULL, NULL, &status);
      shift_from_symbol = (char*)buffer[i] - (char*)info.dli_saddr;

      // The pattern is:
      // - file name of the object
      // - demangled or original name of the obect (or "(null)" if NULL)
      // - address of the object
      // - distance to the closest symbol
      num_bytes = snprintf(message_buffer, message_buffer_size,
                           "%s %s %p + %zd\n", info.dli_fname,
                           status == 0 ? demangled : info.dli_sname, buffer[i],
                           info.dli_saddr != NULL ? shift_from_symbol : 0);
      free(demangled);
      WriteBuffer(message_buffer, num_bytes, message_buffer_size);

    } else {
      // Failed to translate the address into symbolic information,
      // at least print the raw address value.
      num_bytes = snprintf(message_buffer, message_buffer_size, "%p\n", buffer[i]);
      WriteBuffer(message_buffer, num_bytes, message_buffer_size);
    }
  }

  if (n_frames == max_frames)
    write(STDERR_FILENO, "[truncated]\n", 14);

  // Write a message to stderr using only reentrant functions.
  num_bytes = snprintf(message_buffer, message_buffer_size,
                       "Caught signal %d (%s): %s\n",
                       signum, collector::SignalName(signum), strsignal(signum));
  WriteBuffer(message_buffer, num_bytes, message_buffer_size);

  // Re-raise the signal (this time routing it to the default handler) to make
  // sure we get the correct exit code.
  signal(signum, SIG_DFL);
  raise(signum);
}
