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

#include "ConnScraper.h"

#include <fcntl.h>
#include <netinet/tcp.h>

#include <cctype>
#include <cinttypes>
#include <cstring>

#include "Containers.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

namespace {

// String parsing helper functions

// rep_find applies find n times, always advancing past the found character in each subsequent application.
StringView::size_type rep_find(int n, StringView str, char c) {
  if (n <= 0) return StringView::npos;

  StringView::size_type pos = 0;
  while (--n > 0) {
    pos = str.find(c, pos);
    if (pos == StringView::npos) return StringView::npos;
    pos++;
  }
  return str.find(c, pos);
}

// nextfield advances to the next field in a space-delimited string.
const char *nextfield(const char* p, const char* endp) {
  while (p < endp && *p && !std::isspace(*p)) p++;
  while (p < endp && *p && std::isspace(*++p));
  return (p < endp && *p) ? p : nullptr;
}

// rep_nextfield repeatedly applies nextfield n times.
const char *rep_nextfield(int n, const char* p, const char* endp) {
  while (n-- > 0 && p) {
    p = nextfield(p, endp);
  }
  return p;
}

// General functions for reading data from /proc

// ReadINode reads the inode from the symlink of form '<prefix>:[<inode>]' of the given path. If an error is encountered
// or the inode symlink doesn't have the correct prefix, false is returned.
bool ReadINode(int dirfd, const char* path, const char* prefix, ino_t* inode) {
  char linkbuf[64];
  ssize_t nread = readlinkat(dirfd, path, linkbuf, sizeof(linkbuf));
  if (nread <= 0 || nread >= ssizeof(linkbuf) - 1) return false;
  linkbuf[nread] = '\0';
  if (linkbuf[nread - 1] != ']') return false;

  size_t prefix_len = std::strlen(prefix);
  if (std::strncmp(linkbuf, prefix, prefix_len) != 0) return false;
  if (std::strncmp(linkbuf + prefix_len, ":[", 2) != 0) return false;

  // Parse inode value as decimal
  char* endp;
  uintmax_t parsed = std::strtoumax(linkbuf + prefix_len + 2, &endp, 10);
  if (*endp != ']') return false;
  *inode = static_cast<ino_t>(parsed);
  return true;
}

// GetNetworkNamespace returns the inode of the network namespace of the process represented by the given proc
// directory.
bool GetNetworkNamespace(int dirfd, ino_t* inode) {
  return ReadINode(dirfd, "ns/net", "net", inode);
}

// GetSocketINodes returns a list of all socket inodes associated with open file descriptors of the process represented
// by dirfd.
bool GetSocketINodes(int dirfd, UnorderedSet<ino_t>* sock_inodes) {
  DirHandle fd_dir = FDHandle(openat(dirfd, "fd", O_RDONLY));
  if (!fd_dir.valid()) {
    CLOG(ERROR) << "could not open fd directory";
    return false;
  }

  while (auto curr = fd_dir.read()) {
    if (!std::isdigit(curr->d_name[0])) continue;  // only look at fd entries, ignore '.' and '..'.

    ino_t inode;
    if (!ReadINode(fd_dir.fd(), curr->d_name, "socket", &inode)) continue;  // ignore non-socket fds
    sock_inodes->insert(inode);
  }

  return true;
}

// IsContainerID returns whether the given string view represents a container ID.
bool IsContainerID(StringView str) {
  if (str.size() != 64) return false;
  for (auto c : str) {
    if (!std::isdigit(c) && (c < 'a' || c > 'f')) return false;
  }
  return true;
}

// GetContainerID retrieves the container ID of the process represented by dirfd. The container ID is extracted from
// the cgroup.
bool GetContainerID(int dirfd, std::string* container_id) {
  FileHandle cgroups_file(FDHandle(openat(dirfd, "cgroup", O_RDONLY)), "r");
  if (!cgroups_file.valid()) return false;

  thread_local char* linebuf;
  thread_local size_t linebuf_cap;

  ssize_t line_len;
  while ((line_len = getline(&linebuf, &linebuf_cap, cgroups_file.get())) != -1) {
    if (!line_len) continue;
    if (linebuf[line_len - 1] == '\n') line_len--;

    StringView line(linebuf, line_len);
    auto short_container_id = ExtractContainerID(line);
    if (!short_container_id) continue;

    *container_id = short_container_id.str();
    return true;
  }

  return false;
}

// Functions for parsing `net/tcp[6]` files

// IsHexChar checks if the given character is an (uppercase) hexadecimal character.
bool IsHexChar(char c) {
  if (std::isdigit(c)) return true;
  return c >= 'A' && c <= 'F';
}

// HexCharToVal returns the numeric value of a single (uppercase) hexadecimal digit.
unsigned char HexCharToVal(char c) {
  if (std::isdigit(c)) return c - '0';
  return 10 + (c - 'A');
}

// ReadHexBytes reads bytes in (uppercase) hexadecimal representation into buf. It does so in chunks of size chunk_size.
// If reverse is true, the individual chunks are reversed (however, not the entire sequence of bytes). The return value
// is the number of *bytes* read from the input (i.e., to correctly advance p after reading, it has to be incremented
// by twice the return value).
int ReadHexBytes(const char* p, const char* endp, void* buf, int chunk_size, int num_chunks, bool reverse) {
  int i = 0;
  int num_bytes = chunk_size * num_chunks;
  auto bbuf = static_cast<uint8_t*>(buf);

  for (; i < num_bytes && p < endp - 2; i++) {
    char high = *p++;
    char low = *p++;
    if (!IsHexChar(high) || !IsHexChar(low)) break;
    *bbuf++ = HexCharToVal(high) << 4 | HexCharToVal(low);

    if (reverse && i % chunk_size == chunk_size - 1) {
      // Reverse current chunk
      std::reverse(bbuf - chunk_size, bbuf);
    }
  }

  return i;
}

// ConnLineData is the interesting (for our purposes) subset of the data stored in a single (non-header) line of
// `net/tcp[6]`.
struct ConnLineData {
  Endpoint local;
  Endpoint remote;
  uint8_t state;
  ino_t inode;
};

struct ConnInfo {
  Endpoint local;
  Endpoint remote;
  L4Proto l4proto;
  bool is_server;
};

// ParseEndpoint parses an endpoint listed in the `net/tcp[6]` file.
const char *ParseEndpoint(const char* p, const char* endp, Address::Family family, Endpoint* endpoint) {
  static bool needs_byteorder_swap = (htons(42) != 42);

  std::array<uint8_t, Address::kMaxLen> addr_data = {};

  int addr_len = Address::Length(family);
  int nread = ReadHexBytes(p, endp, addr_data.data(), 4, addr_len/4, needs_byteorder_swap);
  if (nread != addr_len) return nullptr;
  p += nread * 2;
  if (*p++ != ':') return nullptr;

  uint16_t port;
  nread = ReadHexBytes(p, endp, &port, sizeof(port), 1, needs_byteorder_swap);
  if (nread != sizeof(port)) return nullptr;
  p += nread * 2;

  *endpoint = Endpoint(Address(family, addr_data), port);
  return p;
}

// ParseConnLine parses an entire line in the `net/tcp[6]` file.
bool ParseConnLine(const char* p, const char* endp, Address::Family family, ConnLineData* data) {
  // Strip leading spaces.
  while (std::isspace(*p)) p++;

  // 0: sl

  p = nextfield(p, endp);
  if (!p) return false;
  // 1: local_address
  p = ParseEndpoint(p, endp, family, &data->local);
  if (!p) return false;

  p = nextfield(p, endp);
  if (!p) return false;
  // 2: rem_address
  p = ParseEndpoint(p, endp, family, &data->remote);
  if (!p) return false;

  p = nextfield(p, endp);
  if (!p) return false;
  // 3: st
  int nread = ReadHexBytes(p, endp, &data->state, 1, 1, false);
  if (nread != 1) return false;
  p += nread * 2;

  p = rep_nextfield(6, p, endp);
  if (!p) return false;
  // 9: inode
  char* parse_endp;
  uintmax_t inode = strtoumax(p, &parse_endp, 10);
  if (*parse_endp && !std::isspace(*parse_endp)) return false;
  data->inode = static_cast<ino_t>(inode);

  return true;
}

// LocalIsServer returns true if the connection between local and remote looks like the local end is the server (taking
// the set of listening endpoints into account), and false otherwise.
bool LocalIsServer(const Endpoint& local, const Endpoint& remote, const UnorderedSet<Endpoint>& listen_endpoints) {
  if (Contains(listen_endpoints, local)) return true;

  // Check if we are listening on the given port on any interface.
  Endpoint local_any(Address::Any(local.address().family()), local.port());
  if (Contains(listen_endpoints, local_any)) return true;

  // We didn't find an entry for listening on this address, but closing a listen socket does not terminate established
  // connections. We hence have to resort to inspecting the port number to see which one seems more likely to be
  // ephemeral.
  return IsEphemeralPort(remote.port()) > IsEphemeralPort(local.port());
}

// ReadConnectionsFromFile reads all connections from a `net/tcp[6]` file and stores them by inode in the given map.
bool ReadConnectionsFromFile(Address::Family family, L4Proto l4proto, std::FILE* f,
                             UnorderedMap<ino_t, ConnInfo>* connections) {
  char line[512];

  if (!std::fgets(line, sizeof(line), f)) return false;  // ignore the first *header) line.

  UnorderedSet<Endpoint> listen_endpoints;

  while (std::fgets(line, sizeof(line), f)) {
    ConnLineData data;
    if (!ParseConnLine(line, line + sizeof(line), family, &data)) continue;
    if (data.state == TCP_LISTEN) {  // listen socket
      listen_endpoints.insert(data.local);
      continue;
    }
    if (data.state != TCP_ESTABLISHED) {
      continue;
    }

    if (!data.inode) continue;  // socket was closed or otherwise unavailable
    auto& conn_info = (*connections)[data.inode];
    conn_info.local = data.local;
    conn_info.remote = data.remote;
    conn_info.l4proto = l4proto;
    // Note that the layout of net/tcp guarantees that all listen sockets will be listed before all active or closed
    // connections, hence we can assume listen_endpoint to have its final value at this point.
    conn_info.is_server = LocalIsServer(data.local, data.remote, listen_endpoints);
  }

  return true;
}

// GetConnections reads all active connections (inode -> connection info mapping) for a given network NS, addressed by
// the dir FD for a proc entry of a process in that network namespace.
bool GetConnections(int dirfd, UnorderedMap<ino_t, ConnInfo>* connections) {
  bool success = true;
  {
    FDHandle net_tcp_fd = openat(dirfd, "net/tcp", O_RDONLY);
    if (net_tcp_fd.valid()) {
      FileHandle net_tcp(std::move(net_tcp_fd), "r");
      success = ReadConnectionsFromFile(Address::Family::IPV4, L4Proto::TCP, net_tcp, connections) && success;
    } else {
      success = false;  // there should always be a net/tcp file
    }
  }

  {
    FDHandle net_tcp6_fd = openat(dirfd, "net/tcp6", O_RDONLY);
    if (net_tcp6_fd.valid()) {
      FileHandle net_tcp6(std::move(net_tcp6_fd), "r");
      success = ReadConnectionsFromFile(Address::Family::IPV6, L4Proto::TCP, net_tcp6, connections) && success;
    } else {
      success = false;
    }
  }

  return success;
}

// netns -> (inode -> connection info) mapping
using ConnsByNS = UnorderedMap<ino_t, UnorderedMap<ino_t, ConnInfo>>;
// container id -> (netns -> socket inodes) mapping
using SocketsByContainer = UnorderedMap<std::string, UnorderedMap<ino_t, UnorderedSet<ino_t>>>;

// ResolveSocketInodes takes a netns -> (inode -> connection info) mapping and a
// container id -> (netns -> socket inodes) mapping, and synthesizes this to a list of (container id, connection info)
// tuples.
void ResolveSocketInodes(const SocketsByContainer& sockets_by_container, const ConnsByNS& conns_by_ns,
                         std::vector<Connection>* connections) {
  for (const auto& container_sockets : sockets_by_container) {
    const auto& container_id = container_sockets.first;
    for (const auto& netns_sockets : container_sockets.second) {
      const auto* conns = Lookup(conns_by_ns, netns_sockets.first);
      if (!conns) continue;
      for (const auto& socket_inode : netns_sockets.second) {
        const auto* conn = Lookup(*conns, socket_inode);
        if (!conn) continue;
        Connection connection(container_id, conn->local, conn->remote, conn->l4proto, conn->is_server);
        if (!IsRelevantConnection(connection)) continue;
        connections->push_back(std::move(connection));
      }
    }
  }
}

// ReadContainerConnections reads all container connection info from the given `/proc`-like directory. All connections
// from non-container processes are ignored.
bool ReadContainerConnections(const char* proc_path, std::vector<Connection>* connections) {
  DirHandle procdir = opendir(proc_path);
  if (!procdir.valid()) {
    CLOG(ERROR) << "Could not open " << proc_path << ": " << StrError();
    return false;
  }

  ConnsByNS conns_by_ns;
  SocketsByContainer sockets_by_container_and_ns;

  // Read all the information from proc.
  while (auto curr = procdir.read()) {
    if (!std::isdigit(curr->d_name[0])) continue;  // only look for <pid> entries

    FDHandle dirfd = procdir.openat(curr->d_name, O_RDONLY);
    if (!dirfd.valid()) {
      CLOG(DEBUG) << "Could not open process directory " << curr->d_name << ": " << StrError();
      continue;
    }

    std::string container_id;
    if (!GetContainerID(dirfd, &container_id)) continue;

    uint64_t netns_inode;
    if (!GetNetworkNamespace(dirfd, &netns_inode)) {
      CLOG(ERROR) << "Could not determine network namespace: " << StrError();
      continue;
    }

    auto& container_ns_sockets = sockets_by_container_and_ns[container_id][netns_inode];
    bool no_sockets = container_ns_sockets.empty();

    if (!GetSocketINodes(dirfd, &container_ns_sockets)) {
      CLOG(ERROR) << "Could not obtain socket inodes: " << StrError();
      continue;
    }

    if (no_sockets && !container_ns_sockets.empty()) {
      // These are the first sockets for this (container, netns) pair. Make sure we actually have the information about
      // connections in this network namespace.
      auto emplace_res = conns_by_ns.emplace(netns_inode, UnorderedMap<uint64_t, ConnInfo>());
      if (emplace_res.second) {
        if (!GetConnections(dirfd, &emplace_res.first->second)) {
          // If there was an error reading connections, that could be due to a number of reasons.
          // We need to differentiate persistent errors (e.g., expected net/tcp6 file not found)
          // from spurious/race condition errors caused by the process disappearing while reading
          // the directory. To determine if the latter is the root cause, we reattempt to read the
          // network namespace inode; if that succeeds, we assume that the process is still alive
          // and any errors encountered are persistent.
          uint64_t netns_inode2;
          if (!GetNetworkNamespace(dirfd, &netns_inode2) || netns_inode2 != netns_inode) {
            conns_by_ns.erase(emplace_res.first);
            continue;
          }
        }
      }
    }
  }

  ResolveSocketInodes(sockets_by_container_and_ns, conns_by_ns, connections);
  return true;
}

}  // namespace

StringView ExtractContainerID(StringView cgroup_line) {
  auto start = rep_find(2, cgroup_line, ':');
  if (start == StringView::npos) return {};
  StringView cgroup_path = cgroup_line.substr(start + 1);

  if (cgroup_path.substr(cgroup_path.size() - StrLen(".scope")) == ".scope") {
    cgroup_path.remove_suffix(StrLen(".scope"));
  }

  auto container_id_part = cgroup_path.substr(cgroup_path.size() - 65);
  if (container_id_part.size() != 65) return {};
  if (container_id_part[0] != '/' && container_id_part[0] != '-') return {};
  container_id_part.remove_prefix(1);

  if (!IsContainerID(container_id_part)) return {};
  return container_id_part.substr(0, 12);
}

bool ConnScraper::Scrape(std::vector<Connection>* connections) {
  return ReadContainerConnections(proc_path_.c_str(), connections);
}

}  // namespace collector
