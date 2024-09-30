#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bits/getopt_ext.h>
#include <sys/socket.h>
#include <sys/types.h>

static bool running = true;
static const char LOREM_IPSUM[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "
    "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. "
    "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n";
static const int LOREM_IPSUM_LEN = sizeof(LOREM_IPSUM) / sizeof(char);
static const size_t IOVEC_N = 32;
static const size_t MMSGHDR_N = 32;

typedef enum send_method_e {
  SENDTO = 0,
  SENDMSG = 1,
  SENDMMSG = 2,
} send_method_t;

typedef struct target_array_s {
  struct addrinfo** ptr;
  unsigned int len;
} target_array_t;

typedef struct config_s {
  int ai_family;
  int period;
  send_method_t syscall;
  unsigned int msgs_per_target;
} config_t;

typedef struct run_data_s {
  send_method_t syscall;
  target_array_t targets;
  int fd;
  int period;
  unsigned int msgs_per_target;
} run_data_t;

void usage(char* prog) {
  printf("%s [FLAGS] <IP> [... IP]\n", prog);
  printf("\n<IP>:\n\tThe IP of the server to send messages to.\n\tCan specify multiple destinations.");
  printf("\n[FLAGS]:\n");
  printf("  -6, --ipv6\n\tUse IPv6 instead of IPv4, the provided IP must be in the correct format\n\n");
  printf("  -h, --help\n\tShow this help message\n\n");
  printf("  -p, --period=2\n\tPeriod used to send data\n\n");
  printf("  -s, --syscall=\"sendto\"\n\tSyscall to be used for sending messages\n\tOne of: sendto, sendmsg, sendmmsg\n\n");
  printf("  -m, --messages=32\n\tMessages to send to each target when using sendmmsg\n\n");
}

void handle_stop(int sig) {
  running = false;
}

void iovec_free(struct iovec* iov) {
  free(iov);
}

struct iovec* iovec_new() {
  struct iovec* iov = calloc(IOVEC_N, sizeof(struct iovec));
  if (iov == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < IOVEC_N; i++) {
    iov[i].iov_base = (void*)LOREM_IPSUM;
    iov[i].iov_len = LOREM_IPSUM_LEN;
  }

  return iov;
}

void mmsghdr_free(struct mmsghdr* mmh, unsigned int len, unsigned int msgs_per_target) {
  if (mmh == NULL) {
    return;
  }

  for (int i = 0; i < len * msgs_per_target; i++) {
    iovec_free(mmh[i].msg_hdr.msg_iov);
  }

  free(mmh);
}

struct mmsghdr* mmsghdr_new(target_array_t* targets, unsigned int msgs_per_target) {
  assert(targets != NULL);

  struct mmsghdr* mmh = calloc(targets->len * msgs_per_target, sizeof(struct mmsghdr));
  if (mmh == NULL) {
    return NULL;
  }

  for (int i = 0; i < targets->len; i++) {
    struct addrinfo* server = targets->ptr[i];

    for (int j = 0; j < msgs_per_target; j++) {
      struct msghdr* mh = &mmh[j + (i * msgs_per_target)].msg_hdr;
      mh->msg_iov = iovec_new();
      if (mh->msg_iov == NULL) {
        goto fail;
      }
      mh->msg_iovlen = IOVEC_N;
      mh->msg_name = server->ai_addr;
      mh->msg_namelen = server->ai_addrlen;
    }
  }

  return mmh;

fail:
  mmsghdr_free(mmh, targets->len, msgs_per_target);
  return NULL;
}

ssize_t send_sendto(run_data_t* data) {
  assert(data != NULL);

  for (int i = 0; i < data->targets.len; i++) {
    struct addrinfo* addr = data->targets.ptr[i];

    if (sendto(data->fd, LOREM_IPSUM, LOREM_IPSUM_LEN, 0, addr->ai_addr, addr->ai_addrlen) < 0) {
      return errno;
    }
  }
  return 0;
}

ssize_t send_sendmsg(run_data_t* data) {
  assert(data != NULL);

  int err = 0;
  for (int i = 0; i < data->targets.len; i++) {
    struct addrinfo* addr = data->targets.ptr[i];

    struct iovec* iov = iovec_new();
    if (iov == NULL) {
      return ENOMEM;
    }

    struct msghdr mh = {
        .msg_name = addr->ai_addr,
        .msg_namelen = addr->ai_addrlen,
        .msg_iov = iov,
        .msg_iovlen = IOVEC_N,
    };

    if (sendmsg(data->fd, &mh, 0) < 0) {
      err = errno;
    }

    iovec_free(iov);

    if (err != 0) {
      break;
    }
  }
  return err;
}

ssize_t send_sendmmsg(run_data_t* data) {
  assert(data != NULL);

  // size that mmh will have on return from mmsghdr_new.
  unsigned int vlen = data->targets.len * data->msgs_per_target;
  struct mmsghdr* mmh = mmsghdr_new(&data->targets, data->msgs_per_target);
  if (mmh == NULL) {
    return ENOMEM;
  }

  int err = 0;
  if (sendmmsg(data->fd, mmh, vlen, 0) < 0) {
    err = errno;
  }

  mmsghdr_free(mmh, data->targets.len, data->msgs_per_target);
  return err;
}

ssize_t m_send(run_data_t* data) {
  printf("Sending data...\n");
  switch (data->syscall) {
    case SENDTO:
      return send_sendto(data);
    case SENDMSG:
      return send_sendmsg(data);
    case SENDMMSG:
      return send_sendmmsg(data);
    default:
      fprintf(stderr, "Invalid syscall\n");
      return -1;
  }
}

struct addrinfo** get_targets(int argc, char** argv, int n_servers, int ai_family) {
  struct addrinfo** targets = (struct addrinfo**)calloc(n_servers, sizeof(struct addrinfo*));
  if (targets == NULL) {
    fprintf(stderr, "Failed to reserve memory for target servers: (%d) %s", errno, strerror(errno));
    return NULL;
  }

  struct addrinfo hints = {
      .ai_family = ai_family,
      .ai_socktype = SOCK_DGRAM,
  };

  for (int i = 0; i < n_servers; i++) {
    char* ip = argv[optind + i];
    char* port = NULL;
    if (ai_family == AF_INET6) {
      port = strrchr(ip, ']');
      if (port == NULL) {
        port = "9090";
      } else if (*ip != '[' || *(port + 1) != ':') {
        fprintf(stderr, "Invalid IPv6 address\n");
        goto error;
      } else {
        ip++;
        *port = '\0';
        port += 2;
      }
    } else {
      port = strrchr(ip, ':');
      if (port != NULL) {
        *port = '\0';
        port++;
      } else {
        port = "9090";
      }
    }

    struct addrinfo* servinfo = NULL;
    ssize_t ret = getaddrinfo(ip, port, &hints, &servinfo);

    if (ret != 0) {
      fprintf(stderr, "getaddrinfo failed: %zd\n", ret);
      goto error;
    }

    targets[i] = servinfo;
  }

  return targets;

error:
  free(targets);
  return NULL;
}

int run(run_data_t* data) {
  assert(data != NULL);

  while (running) {
    ssize_t ret = m_send(data);
    if (ret != 0) {
      fprintf(stderr, "send failed: (%zd) %s\n", ret, strerror(ret));
      return -1;
    }

    sleep(data->period);
  }
  return 0;
}

int main(int argc, char* argv[]) {
  struct option options[] = {
      {.name = "ipv6", .has_arg = no_argument, .flag = NULL, .val = '6'},
      {.name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h'},
      {.name = "period", .has_arg = required_argument, .flag = NULL, .val = 'p'},
      {.name = "syscall", .has_arg = required_argument, .flag = NULL, .val = 's'},
      {.name = "messages", .has_arg = required_argument, .flag = NULL, .val = 'm'},
      {.name = 0, .has_arg = 0, .flag = 0, .val = 0}};
  int opt = 0;
  config_t config = {
      .ai_family = AF_INET,
      .period = 2,
      .syscall = SENDTO,
      .msgs_per_target = MMSGHDR_N,
  };

  while ((opt = getopt_long(argc, argv, "6hp:s:m:", options, NULL)) != -1) {
    switch (opt) {
      case '6':
        config.ai_family = AF_INET6;
        break;

      case 'h':
        usage(argv[0]);
        return 0;

      case 'p':
        config.period = atoi(optarg);
        break;

      case 's':
        if (strcmp(optarg, "sendto") == 0) {
          config.syscall = SENDTO;
        } else if (strcmp(optarg, "sendmsg") == 0) {
          config.syscall = SENDMSG;
        } else if (strcmp(optarg, "sendmmsg") == 0) {
          config.syscall = SENDMMSG;
        } else {
          fprintf(stderr, "Unknown send method: %s\n", optarg);
          usage(argv[0]);
          return -1;
        }
        break;

      case 'm':
        config.msgs_per_target = atoi(optarg);
        break;

      default:
        fprintf(stderr, "Unknown option %s\n", argv[optind]);
        usage(argv[0]);
        return -1;
    }
  }

  int n_servers = argc - optind;
  if (n_servers <= 0) {
    fprintf(stderr, "Missing required argument\n");
    usage(argv[0]);
    return -1;
  }

  struct addrinfo** targets = get_targets(argc, argv, n_servers, config.ai_family);
  if (targets == NULL) {
    return -1;
  }

  int fd = socket(config.ai_family, SOCK_DGRAM, 0);
  if (fd < 0) {
    fprintf(stderr, "Failed to create socket: (%d) %s\n", errno, strerror(errno));
    return -1;
  }

  run_data_t run_data = {
      .syscall = config.syscall,
      .targets = {.ptr = targets, .len = n_servers},
      .fd = fd,
      .period = config.period,
      .msgs_per_target = config.msgs_per_target,
  };

  signal(SIGTERM, handle_stop);
  signal(SIGINT, handle_stop);

  int ret = run(&run_data);

  for (int i = 0; i < run_data.targets.len; i++) {
    struct addrinfo* addr = run_data.targets.ptr[i];
    freeaddrinfo(addr);
    close(run_data.fd);
  }
  free(run_data.targets.ptr);

  return ret;
}
