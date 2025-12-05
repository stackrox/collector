#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <bits/getopt_ext.h>
#include <sys/socket.h>
#include <sys/types.h>

static const size_t BUF_SIZE = 4096;
static const size_t IOVEC_N = 16;
static const size_t MMSGHDR_N = 32;
static bool running = true;

typedef enum recv_method_e {
  RECVFROM = 0,
  RECVMSG = 1,
  RECVMMSG = 2,
} recv_method_t;

void usage(char* prog) {
  printf("%s [FLAGS]\n", prog);
  printf("\nFLAGS:\n");
  printf("  -6, --ipv6:\n\tUse IPv6 instead of IPv4\n\n");
  printf("  -h, --help:\n\tShow this help message\n\n");
  printf("  -p, --port=\"9090\":\n\tUse the specified port\n\n");
  printf("  -s, --syscall=\"recvfrom\":\n\tSyscall to be used for receiving messages\n\tOne of: recvfrom, recvmsg, recvmmsg\n\n");
}

void handle_stop(int sig) {
  running = false;
}

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void iovec_free(struct iovec* iov) {
  if (iov == NULL) {
    return;
  }

  for (int i = 0; i < IOVEC_N; i++) {
    free(iov[i].iov_base);
  }
  free(iov);
}

struct iovec* iovec_new() {
  struct iovec* ret = calloc(IOVEC_N, sizeof(struct iovec));
  if (ret == NULL) {
    return NULL;
  }

  for (int i = 0; i < IOVEC_N; i++) {
    struct iovec* iov = &ret[i];
    iov->iov_base = malloc(BUF_SIZE + 1);
    if (iov->iov_base == NULL) {
      goto fail;
    }
    iov->iov_len = BUF_SIZE;
  }

  return ret;

fail:
  iovec_free(ret);
  return NULL;
}

void mmsghdr_free(struct mmsghdr* mmh) {
  if (mmh == NULL) {
    return;
  }

  for (int i = 0; i < MMSGHDR_N; i++) {
    struct msghdr* mh = &mmh[i].msg_hdr;
    free(mh->msg_name);
    iovec_free(mh->msg_iov);
  }

  free(mmh);
}

struct mmsghdr* mmsghdr_new() {
  struct mmsghdr* mmh = calloc(MMSGHDR_N, sizeof(struct mmsghdr));
  if (mmh == NULL) {
    return NULL;
  }

  for (int i = 0; i < MMSGHDR_N; i++) {
    struct msghdr* mh = &mmh[i].msg_hdr;
    mh->msg_namelen = sizeof(struct sockaddr);
    mh->msg_name = malloc(mh->msg_namelen);
    if (mh->msg_name == NULL) {
      goto fail;
    }

    mh->msg_iov = iovec_new();
    if (mh->msg_iov == NULL) {
      goto fail;
    }
    mh->msg_iovlen = IOVEC_N;
  }

  return mmh;

fail:
  mmsghdr_free(mmh);
  return NULL;
}

void print_msg(struct msghdr* mh, ssize_t remaining) {
  struct sockaddr* from = mh->msg_name;
  struct iovec* iov = mh->msg_iov;
  char s[INET6_ADDRSTRLEN];

  for (int i = 0; i < mh->msg_iovlen && remaining >= 0; i++) {
    char* buf = iov[i].iov_base;
    size_t terminator_index = remaining < iov[i].iov_len ? remaining : iov[i].iov_len;
    buf[terminator_index] = '\0';

    printf("%s: %s\n", inet_ntop(from->sa_family, get_in_addr(from), s, sizeof(s)), buf);
    fflush(stdout);

    remaining -= iov[i].iov_len;
  }
}

ssize_t receive_recvfrom(int fd) {
  char buf[BUF_SIZE + 1];
  char s[INET6_ADDRSTRLEN];
  struct sockaddr from;
  socklen_t fromlen = sizeof(from);
  ssize_t res = 0;

  res = recvfrom(fd, buf, BUF_SIZE, 0, &from, &fromlen);
  if (res < 0) {
    if (errno == EINTR || errno == EAGAIN) {
      return 0;
    }
    return errno;
  }

  buf[res] = '\0';
  printf("%s: %s\n", inet_ntop(from.sa_family, get_in_addr(&from), s, sizeof(s)), buf);
  fflush(stdout);

  return 0;
}

ssize_t receive_recvmsg(int fd) {
  ssize_t res = 0;
  int err = 0;
  struct sockaddr from;
  socklen_t fromlen = sizeof(from);
  struct iovec* iov = iovec_new();
  if (iov == NULL) {
    return ENOMEM;
  }
  struct msghdr mh = {
      .msg_name = &from,
      .msg_namelen = fromlen,
      .msg_iov = iov,
      .msg_iovlen = IOVEC_N,
  };

  res = recvmsg(fd, &mh, 0);
  if (res < 0) {
    if (errno != EINTR && errno != EAGAIN) {
      err = errno;
    }
    goto end;
  }

  print_msg(&mh, res);

end:
  iovec_free(iov);
  return err;
}

ssize_t receive_recvmmsg(int fd) {
  ssize_t res = 0;
  int err = 0;
  struct mmsghdr* mmh = mmsghdr_new();
  if (mmh == NULL) {
    return ENOMEM;
  }

  res = recvmmsg(fd, mmh, MMSGHDR_N, 0, NULL);
  if (res < 0) {
    if (errno != EINTR && errno != EAGAIN) {
      err = errno;
    }
    goto end;
  }

  for (int i = 0; i < res; i++) {
    struct msghdr* mh = &mmh[i].msg_hdr;
    ssize_t size = mmh[i].msg_len;

    print_msg(mh, size);
  }

end:
  mmsghdr_free(mmh);
  return err;
}

ssize_t receive(int fd, recv_method_t syscall) {
  switch (syscall) {
    case RECVFROM:
      return receive_recvfrom(fd);
    case RECVMSG:
      return receive_recvmsg(fd);
    case RECVMMSG:
      return receive_recvmmsg(fd);
    default:
      fprintf(stderr, "Invalid syscall\n");
      return -1;
  }
}

int run(int ai_family, const char* port, recv_method_t syscall) {
  int fd = -1;
  struct addrinfo hints = {
      .ai_family = ai_family,
      .ai_socktype = SOCK_DGRAM,
      .ai_flags = AI_PASSIVE,
  };
  struct addrinfo* res = NULL;
  ssize_t ret = getaddrinfo(NULL, port, &hints, &res);

  if (ret != 0) {
    fprintf(stderr, "getaddrinfo failed: %zd\n", ret);
    return -1;
  }

  struct addrinfo* p = res;
  for (; p != NULL; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) {
      fprintf(stderr, "Failed to create socket: (%d) %s\n", errno, strerror(errno));
      continue;
    }

    if (bind(fd, res->ai_addr, res->ai_addrlen) != 0) {
      fprintf(stderr, "bind failed: (%d) %s\n", errno, strerror(errno));
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "failed to create socket\n");
    return -1;
  }

  freeaddrinfo(res);

  struct timeval tv = {
      .tv_sec = 1, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

  while (running) {
    ret = receive(fd, syscall);
    if (ret < 0) {
      fprintf(stderr, "receive failed: (%zd) %s\n", ret, strerror(ret));
      continue;
    }
  }

  close(fd);
  return 0;
}

int main(int argc, char* argv[]) {
  struct option options[] = {
      {.name = "ipv6", .has_arg = no_argument, .flag = NULL, .val = '6'},
      {.name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h'},
      {.name = "port", .has_arg = required_argument, .flag = NULL, .val = 'p'},
      {.name = "syscall", .has_arg = required_argument, .flag = NULL, .val = 's'},
      {.name = 0, .has_arg = 0, .flag = 0, .val = 0}};
  int opt = 0;
  int ai_family = AF_INET;
  char* port = "9090";
  recv_method_t syscall = RECVFROM;

  while ((opt = getopt_long(argc, argv, "6hp:s:", options, NULL)) != -1) {
    switch (opt) {
      case '6':
        ai_family = AF_INET6;
        break;
      case 'h':
        usage(argv[0]);
        return 0;
      case 'p':
        port = optarg;
        printf("Using port %s\n", port);
        break;
      case 's':
        if (strcmp(optarg, "recvfrom") == 0) {
          syscall = RECVFROM;
        } else if (strcmp(optarg, "recvmsg") == 0) {
          syscall = RECVMSG;
        } else if (strcmp(optarg, "recvmmsg") == 0) {
          syscall = RECVMMSG;
        } else {
          fprintf(stderr, "Unknown receive method: %s\n", optarg);
          usage(argv[0]);
          return -1;
        }

        break;
      default:
        fprintf(stderr, "Unknown option %s\n", argv[optind]);
        usage(argv[0]);
        return -1;
    }
  }

  signal(SIGTERM, handle_stop);
  signal(SIGINT, handle_stop);

  return run(ai_family, port, syscall);
}
