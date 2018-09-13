
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <cctype>
#include <cstring>
#include <cstdlib>

#include <array>
#include <string>
#include <unordered_set>

#include "ConnTracker.h"
#include "FileSystem.h"

namespace collector {

// ReadINode reads the inode from the symlink of the given path. If an error is encountered or the inode symlink
// doesn't have the correct prefix, false is returned.
bool ReadINode(int dirfd, const char* path, const char* prefix, uint64_t* inode) {
    char linkbuf[64];
    int nread = readlinkat(dirfd, path, linkbuf, sizeof(linkbuf));
    if (nread <= 0 || nread >= sizeof(linkbuf) - 1) return false;
    linkbuf[nread] = '\0';
    if (linkbuf[nread - 1] != ']') return false;

    int prefix_len = std::strlen(prefix);
    if (std::strncmp(linkbuf, prefix, prefix_len) != 0) return false;
    if (std::strncmp(linkbuf + prefix_len, ":[", 2) != 0) return false;
    char* endp;
    uint64_t parsed = std::strtoul(linkbuf + prefix_len + 2, &endp, 10);
    if (*endp != ']') return false;
    *inode = parsed;
    return true;
}

// GetNetworkNamespace returns the inode of the network namespace of the process represented by the given proc
// directory.
bool GetNetworkNamespace(int dirfd, uint64_t* inode) {
    return ReadINode(dirfd, "ns/net", "net", inode);
}

// GetSocketINodes returns a list of all socket inodes associated with open file descriptors of the process represented
// by dirfd.
bool GetSocketINodes(int dirfd, std::unordered_set<uint64_t>* sock_inodes) {
    FDHandle fd_dir_fd = openat(dirfd, "fd", O_RDONLY);
    if (!fd_dir_fd.valid()) return false;

    DirHandle fd_dir(std::move(fd_dir_fd));
    if (!fd_dir.valid()) return false;

    while (auto curr = fd_dir.read()) {
        if (!isdigit(curr->d_name[0])) continue;  // only look at fd symlinks, ignore '.' and '..'.
        uint64_t inode;
        if (!ReadINode(fd_dir_fd, curr->d_name, "socket", &inode)) continue;
        sock_inodes->insert(inode);
    }
    return true;
}

// GetContainerID retrieves the container ID of the process represented by dirfd.
bool GetContainerID(int dirfd, std::string* container_id) {
    FDHandle cgroups_fd = openat(dirfd, "cgroups", O_RDONLY);
    if (!cgroups_fd.valid()) return false;

    char buf[512];
    int nread = read(cgroups_fd, buf, sizeof(buf) - 1);
    if (nread < 0) return false;

    buf[nread] = '\0';
    char* p = rep_strchr(2, buf, ':');
    if (!p) return false;

    if (strncmp(++p, "/docker/", 8) != 0) return false;
    p += 8;
    if (buf + sizeof(buf) - p < 32) return false;
    *container_id = std::string(p, 32);
    return true;
}

// rep_strchr applies strchr n times, always advancing past the found character.
const char* rep_strchr(int n, const char* str, char c) {
    if (!str) return nullptr;

    str -= 1;
    while (n-- > 0 && str) {
        str = strchr(str + 1, c);
    }
    return str;
}

bool IsHexChar(char c) {
  if (std::isdigit(c)) return true;
  return c >= 'A' && c <= 'F';
}

unsigned char HexCharToVal(char c) {
  if (std::isdigit(c)) return c - '0';
  return c - 'A';
}

int ReadHexBytes(const char* p, const char* endp, void* buf, int num_bytes) {
  int i = 0;
  unsigned char* bbuf = static_cast<unsigned char*>(buf);
  for (; i < num_bytes && p < endp - 2; i++) {
    char high = *p++;
    char low = *p++;
    if (!IsHexChar(high) || !IsHexChar(low)) break;
    *bbuf++ = HexCharToVal(high) << 4 | HexCharToVal(low);
  }

  return i;
}

const char* nextfield(const char* p, const char* endp) {
  while (p < endp && *p && !std::isspace(*p)) p++;
  while (p < endp && *p && std::isspace(*++p));
  return (p < endp && *p) ? p : nullptr;
}

const char* rep_nextfield(int n, const char* p, const char* endp) {
  while (n-- > 0 && p) {
    p = nextfield(p, endp);
  }
  return p;
}

struct ConnLineData {
  Endpoint local;
  Endpoint remote;
  uint64_t inode;
};

struct ConnInfo {
  Endpoint local;
  Endpoint remote;
  L4Proto l4proto;
};

const char* ParseEndpoint(const char* p, const char* endp, Address::Family family, Endpoint* endpoint) {
  std::array<unsigned char, Address::kMaxLen> addr_data;
  int addr_len = Address::Length(family);

  int nread = ReadHexBytes(p, endp, addr_data.data(), addr_len);
  if (nread != addr_len) return nullptr;
  p += nread * 2;
  if (*p++ != ':') return nullptr;

  uint16_t port;
  nread = ReadHexBytes(p, endp, &port, sizeof(port));
  if (nread != sizeof(port)) return nullptr;
  p += nread * 2;

  *endpoint = Endpoint(Address(family, addr_data), ntohs(port));
  return p;
}

bool ParseLine(const char* p, const char* endp, Address::Family family, ConnLineData* data) {
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

  p = rep_nextfield(7, p, endp);
  if (!p) return false;
  // 9: inode
  char* parse_endp;
  uint64_t inode = strtoul(p, &parse_endp, 10);
  if (*parse_endp && !std::isspace(*parse_endp)) return false;
  data->inode = inode;

  return true;
}

bool ReadConnectionsFromFile(Address::Family family, L4Proto l4proto, std::FILE* f, std::unordered_map<uint64_t, ConnInfo>* connections) {
  char line[512];

  if (!std::fgets(line, sizeof(line), f)) return false;

  while (std::fgets(line, sizeof(line), f)) {
    ConnLineData data;
    if (!ParseLine(line, line + sizeof(line), family, &data)) continue;
    if (!data.inode) continue;
    auto& conn_info = (*connections)[data.inode];
    conn_info.local = data.local;
    conn_info.remote = data.remote;
    conn_info.l4proto = l4proto;
  }

  return true;
}
bool GetConnections(int dirfd, std::unordered_map<uint64_t, ConnInfo>* connections) {
  {
    FDHandle net_tcp_fd = openat(dirfd, "net/tcp", O_RDONLY);
    if (!net_tcp_fd.valid()) return false;

    FileHandle net_tcp(std::move(net_tcp_fd), "r");
    if (!ReadConnectionsFromFile(Address::Family::IPV4, L4Proto::TCP, net_tcp, connections)) return false;
  }

  {
    FDHandle net_tcp6_fd = openat(dirfd, "net/tcp6", O_RDONLY);
    if (!net_tcp6_fd.valid()) return false;

    FileHandle net_tcp6(std::move(net_tcp6_fd), "r");
    if (!ReadConnectionsFromFile(Address::Family::IPV6, L4Proto::TCP, net_tcp6, connections)) return false;
  }

  return true;
}

bool ReadContainerConnections(int procdirfd) {
  DirHandle procdir = fdopendir(procdirfd);
  if (!procdir) return false;

  std::unordered_map<uint64_t, std::unordered_map<uint64_t, ConnInfo>> conns_by_ns;

  while (auto curr = procdir.read()) {
    if (!std::isdigit(curr->d_name[0])) continue;  // only look for <pid> entries

    FDHandle dirfd = openat(procdirfd, curr->d_name, O_RDONLY);
    if (!dirfd.valid()) continue;

    std::string container_id;
    if (!GetContainerID(dirfd, &container_id)) continue;

    uint64_t netns_inode;
    if (!GetNetworkNamespace(dirfd, &netns_inode)) continue;

    auto emplace_res = conns_by_ns.emplace(netns_inode, {});
    if (emplace_res.second) {
      if (!GetConnections(dirfd, &emplace_res.first->second)) {
        conns_by_ns.erase(emplace_res.first);
        continue;
      }
    }


  }
  return true;
}

}  // namespace collector