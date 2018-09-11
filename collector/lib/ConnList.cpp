
bool ReadINode(int dirfd, const char* path, const char* prefix, uint64_t* inode) {
    char linkbuf[64];
    int nread = readlinkat(dirfd, path, linkbuf, sizeof(linkbuf));
    if (nread <= 0 || nread >= sizeof(linkbuf)) return false;
    linkbuf[nread] = '\0';
    if (linkbuf[nread - 1] != ']') return false;

    int prefix_len = strlen(prefix);
    if (strncmp(linkbuf, prefix, prefix_len) != 0) return false;
    if (strncmp(linkbuf + prefix_len, ":[", 2) != 0) return false;
    char* endp;
    uint64_t parsed = strtoul(linkbuf + prefix_len + 2, &endp, 10);
    if (*endp != ']') return false;
    *inode = parsed;
    return true;
}

uint64_t GetNetworkNS(int dirfd) {
    char namebuf[17];  // net:[<max 10 digits>] + nul terminator
    int nread = readlinkat(dirfd, "ns/net", namebuf);
    if (nread <= 0 || nread > 16) return 0;
    namebuf[nread] = '\0';
    if (strncmp(namebuf, "net:[", 5) != 0) return 0;
    if (namebuf[nread - 1] != ']') return 0;
    namebuf[nread - 1] = '\0';
    char* endp;
    uint64_t parsed = strtoul(namebuf + 5, &endp, 10);
    if (*endp != '\0') return 0;
    return parsed;
}

bool GetSocketINodes(int dirfd, std::vector<uint64_t>* sock_inodes) {
    int fd_dir_fd = openat(dirfd, "fd");
    if (fd_dir_fd < 0) return false;
    AutoCloser fd_dir_fd_closer(fd_dir_fd);
    DIR* fd_dir = fdopendir(fd_dir_fd);
    if (!fd_dir) return false;

    AutoDIRCloser fd_dir_closer(fd_dir);
    struct dirent* curr;
    while ((curr = readdir(fd_dir)) {
        if (!isdigit(curr->d_name[0])) continue;
        uint64_t inode;
        if (!ReadINode(fd_dir_fd, curr->d_name, "socket", &inode)) continue;
        sock_inodes->push_back(inode);
    }
    return true;
}

std::string GetContainerID(int dirfd) {
    int cgroups_fd = openat(dirfd, "cgroups", O_RDONLY);
    if (cgroups_fd < 0) return "";

    AutoCloser cgroups_fd_closer(&cgroups_fd);

    char buf[512];
    int nread = read(cgroups_fd, buf, sizeof(buf) - 1);
    if (nread < 0) return "";

    buf[nread] = '\0';
    char* p = rep_strchr(2, buf, ':');
    if (!p) return "";

    if (strncmp(++p, "/docker/", 8) != 0) return "";
    p += 8;
    if (buf + sizeof(buf) - p < 32) return "";
    return std::string(p, 32);
}

const char* rep_strchr(int n, const char* str, char c) {
    while (n-- > 0 && str) {
        str = strchr(str, c);
    }
    return str;
}
