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

extern "C" {

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#include <string.h>

}

#include "Network.h"

#include "Logging.h"
#include "Utility.h"

namespace collector {

bool ParseAddress(const std::string& address_str, Address* addr, std::string* error_str) {
  auto delim_pos = address_str.find(':');
  if (delim_pos == std::string::npos) {
    *error_str = "address is not of form <host>:<port>";
    return false;
  }

  addr->host = address_str.substr(0, delim_pos);
  std::string port_str = address_str.substr(delim_pos + 1);
  char* endp;
  long port_num = strtol(port_str.c_str(), &endp, 10);
  if (*endp != '\0') {
    *error_str = Str("non-numeric port part '", port_str, "'");
    return false;
  }
  if (port_num <= 0 || port_num > std::numeric_limits<uint16_t>::max()) {
    *error_str = Str("invalid port number ", port_num);
  }
  addr->port = static_cast<uint16_t>(port_num);
  return true;
}

bool ParseAddressList(const std::string& address_list_str, std::vector<Address>* addresses, std::string* error_str) {
  std::stringstream ss;
  ss.str(address_list_str);
  std::string token;
  while (std::getline(ss, token, ',')) {
    Address addr;
    if (!ParseAddress(token, &addr, error_str)) return false;
    addresses->push_back(std::move(addr));
  }
  return true;
}

namespace {

class AutoCloser {
 public:
  AutoCloser(int* fd) : m_fd(fd) {}
  ~AutoCloser() {
    if (m_fd && *m_fd > 0) {
      int rv = close(*m_fd);
      if (rv != 0) {
        CLOG(WARNING) << "Error closing file descriptor " << *m_fd << ": " << StrError();
      }
    }
  }

 private:
  int* m_fd;
};

}  // namespace

ConnectivityStatus CheckConnectivity(const Address& addr, const std::chrono::milliseconds& timeout,
                                     std::string* error_str, const std::function<bool()>& interrupt, int interrupt_fd) {
  hostent *record = gethostbyname(addr.host.c_str());
  if(record == NULL) {
    *error_str = Str("resolution error: ", hstrerror(h_errno));
    return ConnectivityStatus::ERROR;
  }

  struct sockaddr_in address;
  memcpy(&address.sin_addr.s_addr, record->h_addr, sizeof(in_addr));
  address.sin_family = AF_INET;
  address.sin_port = htons(addr.port);

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    *error_str = Str("cannot open socket: ", StrError());
    return ConnectivityStatus::ERROR;
  }

  AutoCloser closer(&sock);
  int rv = fcntl(sock, F_SETFL, O_NONBLOCK);
  if (rv == -1) {
    *error_str = Str("cannot set socket to nonblocking: ", StrError());
    return ConnectivityStatus::ERROR;
  }

  rv = connect(sock, (struct sockaddr *)&address, sizeof(address));
  if (rv != 0 && errno != EINPROGRESS) {
    *error_str = Str("error connecting: ", StrError());
    return ConnectivityStatus::ERROR;
  }

  constexpr int MAX_NUM_FDS = 2;
  struct pollfd poll_fds[MAX_NUM_FDS] = {
    { sock, POLLIN | POLLOUT, 0 },
    { interrupt_fd, POLLIN, 0 },
  };

  int num_fds = (interrupt_fd != -1) ? 2 : 1;

  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!interrupt()) {
    long msec_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
    if (msec_timeout <= 0) {
      *error_str = "timed out";
      return ConnectivityStatus::ERROR;
    }

    for (int i = 0; i < num_fds; i++) {
      poll_fds[i].revents = 0;
    }

    rv = poll(poll_fds, num_fds, msec_timeout);
    if (rv == 0) {  // timeout
      *error_str = "timed out";
      return ConnectivityStatus::ERROR;
    }

    if (rv == -1) {
      if (errno == EAGAIN) {
        continue;
      }

      *error_str = Str("error when polling: ", StrError());
      return ConnectivityStatus::ERROR;
    }

    // Stop signal
    if (num_fds > 1 && poll_fds[1].revents != 0) {
      return ConnectivityStatus::INTERRUPTED;
    }

    int so_error;
    socklen_t len = sizeof so_error;

    rv = getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (rv != 0) {
      *error_str = Str("error getting socket error: ", StrError());
      return ConnectivityStatus::ERROR;
    }

    if (so_error != 0) {
      *error_str = Str("socket error: ", StrError(so_error));
      return ConnectivityStatus::ERROR;
    }

    // Return true if we can either read from or write to the socket, false otherwise.
    if ((poll_fds[0].revents & (POLLIN | POLLOUT)) == 0) {
      *error_str = "socket not ready";
      return ConnectivityStatus::ERROR;
    }
    return ConnectivityStatus::OK;
  }

  return ConnectivityStatus::INTERRUPTED;
}

}  // namespace collector
