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
#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace collector {

struct Address {
  std::string host;
  uint16_t port;

  Address() = default;
  Address(std::string host, uint16_t port) : host(std::move(host)), port(port) {}
  std::string str() const { return host + ":" + std::to_string(port); }
};

bool ParseAddress(const std::string& address_str, Address* addr, std::string* error_str);
bool ParseAddressList(const std::string& address_list_str, std::vector<Address>* addresses, std::string* error_str);

enum class ConnectivityStatus {
  OK,
  ERROR,
  INTERRUPTED,
};

ConnectivityStatus CheckConnectivity(const Address& address, const std::chrono::milliseconds& timeout,
                                     std::string* error_str,
                                     const std::function<bool()>& interrupt = []{ return false; },
                                     int interrupt_fd = -1);

}  // namespace collector

#endif  // _NETWORK_H_
