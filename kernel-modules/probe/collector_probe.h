#ifndef __COLLECTOR_PROBE_H
#define __COLLECTOR_PROBE_H

/* The structs below have been manually written to their corresponding syscall
 * tracepoint format found in /sys/kernel/tracing/events/syscalls
 * example: cat /sys/kernel/tracing/events/syscalls/sys_enter_setgid/format
 *
 * Note: will rely on implicit padding added by the compiler to ensure fields
 * start at correct offset, otherwise, will explicitly define a pad.
 * example: sys_enter_setgid gid field is defined as long so that it will be aligned
 * on a QWORD boundary.  If defined as int the necessary padding between __syscall_nr
 * and gid will not be added by compiler.
 * name: sys_enter_setgid
 * ID: 185
 * format:
 *	field:unsigned short common_type;	offset:0;	size:2;	signed:0;
 *	field:unsigned char common_flags;	offset:2;	size:1;	signed:0;
 *	field:unsigned char common_preempt_count;	offset:3;	size:1;	signed:0;
 *	field:int common_pid;	offset:4;	size:4;	signed:1;
 *	field:int __syscall_nr;	offset:8;	size:4;	signed:1;
 *	field:gid_t gid;	offset:16;	size:8;	signed:0;
 */
struct sys_enter_accept4_args {
  __u64 pad;
  int __syscall_nr;
  long fd;
  struct sockaddr* upeer_sockaddr;
  int* upeer_len;
  long flags;
};

struct sys_enter_chdir_args {
  __u64 pad;
  int __syscall_nr;
  const char* filename;
};

struct sys_enter_fchdir_args {
  __u64 pad;
  int __syscall_nr;
  long fd;
};

struct sys_enter_close_args {
  __u64 pad;
  int __syscall_nr;
  long fd;
};

struct sys_enter_connect_args {
  __u64 pad;
  int __syscall_nr;
  long fd;
  struct sockaddr* uservaddr;
  long addrlen;
};

struct sys_enter_clone_args {
  __u64 pad;
  int __syscall_nr;
#ifdef __s390x__
  unsigned long newsp;
  unsigned long clone_flags;
  int* parent_tidptr;
  int* child_tidptr;
  unsigned long tls;
#elif __aarch64__  // TODO: verify on aarch64
  unsigned long clone_flags;
  unsigned long newsp;
  int* parent_tidptr;
  unsigned long tls;
  int* child_tidptr;
#else
  unsigned long clone_flags;
  unsigned long newsp;
  int* parent_tidptr;
  int* child_tidptr;
  unsigned long tls;
#endif
};

struct sys_enter_execve_args {
  __u64 pad;
  int __syscall_nr;
  const char* filename;
  const char* argv;
  const char* envp;
};

struct sys_enter_setuid_args {
  __u64 pad;
  int __syscall_nr;
  unsigned long uid;
};

struct sys_enter_setgid_args {
  __u64 pad;
  int __syscall_nr;
  unsigned long gid;
};

struct sys_enter_setresuid_args {
  __u64 pad;
  int __syscall_nr;
  unsigned long ruid;
  unsigned long euid;
  unsigned long suid;
};

struct sys_enter_setresgid_args {
  __u64 pad;
  int __syscall_nr;
  unsigned long rgid;
  unsigned long egid;
  unsigned long sgid;
};

struct sys_enter_socket_args {
  __u64 pad;
  int __syscall_nr;
  long family;
  long type;
  long protocol;
};

#ifdef CAPTURE_SOCKETCALL
struct sys_enter_socketcall_args {
  __u64 pad;
  int __syscall_nr;
  long call;
  unsigned long* args;
};
#endif

struct sys_enter_shutdown_args {
  __u64 pad;
  int __syscall_nr;
  long fd;
  long how;
};

static __always_inline void syscall_to_enter_args(long id, struct sys_enter_args* ctx, struct sys_enter_args* stack_ctx) {
  /* Syscall tracepoints follow their own format (=structure) of arguments.
   * Copying all arguments, e.g. with
   *   memcpy(stack_ctx->args, _READ(ctx->args), sizeof(unsigned long) * NUM_SYS_ENTER_ARGS);
   * would copy beyond the format and results in an EACCESS triggered by
   * format/structure offset checking in the perf event subsystem when installing
   * the BPF program.
   * Also the system exit fillers require access to the arguments to fill event
   * data.  Note that the syscall tracepoints do not provide any argument data
   * in the syscall exit format.  Of course, trying to access argument data in
   * filler results in BPF verifier errors.
   *
   * To provide arguments to the syscall enter and exit fillers, extract the
   * arguments from syscall specific formats/structures to an sys_enter_args
   * structure and use a BPF map to make them available to the enter/exit
   * fillers.
   */
  switch (id) /* maybe use sc_evt here..... */
  {
#ifdef __NR_accept
    case __NR_accept: {
      struct sys_enter_accept4_args* accept4_args = (struct sys_enter_accept4_args*)ctx;
      stack_ctx->args[0] = (unsigned long)accept4_args->fd;
      stack_ctx->args[1] = (unsigned long)accept4_args->upeer_sockaddr;
      stack_ctx->args[2] = (unsigned long)accept4_args->upeer_len;
      break;
    }
#endif
    case __NR_accept4: {
      struct sys_enter_accept4_args* accept4_args = (struct sys_enter_accept4_args*)ctx;
      stack_ctx->args[0] = (unsigned long)accept4_args->fd;
      stack_ctx->args[1] = (unsigned long)accept4_args->upeer_sockaddr;
      stack_ctx->args[2] = (unsigned long)accept4_args->upeer_len;
      stack_ctx->args[3] = (unsigned long)accept4_args->flags;
      break;
    }
    case __NR_connect: {
      struct sys_enter_connect_args* connect_args = (struct sys_enter_connect_args*)ctx;
      stack_ctx->args[0] = (unsigned long)connect_args->fd;
      stack_ctx->args[1] = (unsigned long)connect_args->uservaddr;
      stack_ctx->args[2] = (unsigned long)connect_args->addrlen;
      break;
    }
    case __NR_chdir: {
      struct sys_enter_chdir_args* chdir_args = (struct sys_enter_chdir_args*)ctx;
      stack_ctx->args[0] = (unsigned long)chdir_args->filename;
      break;
    }
    case __NR_fchdir: {
      struct sys_enter_fchdir_args* fchdir_args = (struct sys_enter_fchdir_args*)ctx;
      stack_ctx->args[0] = (unsigned long)fchdir_args->fd;
      break;
    }
    case __NR_clone: {
      struct sys_enter_clone_args* clone_args = (struct sys_enter_clone_args*)ctx;
#ifdef __s390x__
      stack_ctx->args[0] = (unsigned long)clone_args->newsp;
      stack_ctx->args[1] = (unsigned long)clone_args->clone_flags;
      stack_ctx->args[2] = (unsigned long)clone_args->parent_tidptr;
      stack_ctx->args[3] = (unsigned long)clone_args->child_tidptr;
      stack_ctx->args[4] = (unsigned long)clone_args->tls;
#elif __aarch64__
      stack_ctx->args[0] = (unsigned long)clone_args->clone_flags;
      stack_ctx->args[1] = (unsigned long)clone_args->newsp;
      stack_ctx->args[2] = (unsigned long)clone_args->parent_tidptr;
      stack_ctx->args[3] = (unsigned long)clone_args->tls;
      stack_ctx->args[4] = (unsigned long)clone_args->child_tidptr;
#else
      stack_ctx->args[0] = (unsigned long)clone_args->clone_flags;
      stack_ctx->args[1] = (unsigned long)clone_args->newsp;
      stack_ctx->args[2] = (unsigned long)clone_args->parent_tidptr;
      stack_ctx->args[3] = (unsigned long)clone_args->child_tidptr;
      stack_ctx->args[4] = (unsigned long)clone_args->tls;
#endif
      break;
    }
    case __NR_execve: {
      struct sys_enter_execve_args* execve_args = (struct sys_enter_execve_args*)ctx;
      stack_ctx->args[0] = (unsigned long)execve_args->filename;
      stack_ctx->args[1] = (unsigned long)execve_args->argv;
      stack_ctx->args[2] = (unsigned long)execve_args->envp;
      break;
    }
    case __NR_close: {
      struct sys_enter_close_args* close_args = (struct sys_enter_close_args*)ctx;
      stack_ctx->args[0] = (unsigned long)close_args->fd;
      break;
    }
    case __NR_setuid: {
      struct sys_enter_setuid_args* setuid_args = (struct sys_enter_setuid_args*)ctx;
      stack_ctx->args[0] = (unsigned long)setuid_args->uid;
      break;
    }
    case __NR_setgid: {
      struct sys_enter_setgid_args* setgid_args = (struct sys_enter_setgid_args*)ctx;
      stack_ctx->args[0] = (unsigned long)setgid_args->gid;
      break;
    }
    case __NR_setresgid: {
      struct sys_enter_setresgid_args* setresgid_args = (struct sys_enter_setresgid_args*)ctx;
      stack_ctx->args[0] = (unsigned long)setresgid_args->rgid;
      stack_ctx->args[1] = (unsigned long)setresgid_args->egid;
      stack_ctx->args[2] = (unsigned long)setresgid_args->sgid;
      break;
    }
    case __NR_setresuid: {
      struct sys_enter_setresuid_args* setresuid_args = (struct sys_enter_setresuid_args*)ctx;
      stack_ctx->args[0] = (unsigned long)setresuid_args->ruid;
      stack_ctx->args[1] = (unsigned long)setresuid_args->euid;
      stack_ctx->args[2] = (unsigned long)setresuid_args->suid;
      break;
    }
    case __NR_socket: {
      struct sys_enter_socket_args* socket_args = (struct sys_enter_socket_args*)ctx;
      stack_ctx->args[0] = (unsigned long)socket_args->family;
      stack_ctx->args[1] = (unsigned long)socket_args->type;
      stack_ctx->args[2] = (unsigned long)socket_args->protocol;
      break;
    }
#ifdef CAPTURE_SOCKETCALL
    case __NR_socketcall: {
      struct sys_enter_socketcall_args* socketcall_args = (struct sys_enter_socketcall_args*)ctx;
      unsigned long socketcall_id = (unsigned long)socketcall_args->call;

      stack_ctx->args[0] = socketcall_id;
      stack_ctx->args[1] = (unsigned long)socketcall_args->args;

      switch (socketcall_id) {
        case SYS_ACCEPT:
        case SYS_ACCEPT4:
        case SYS_CONNECT:
        case SYS_SHUTDOWN:
        case SYS_SOCKET:
          break;
        default:
          // Filter out any other socket calls
          return;
      }
      break;
    }
#endif
    case __NR_shutdown: {
      struct sys_enter_shutdown_args* shutdown_args = (struct sys_enter_shutdown_args*)ctx;
      stack_ctx->args[0] = (unsigned long)shutdown_args->fd;
      stack_ctx->args[1] = (unsigned long)shutdown_args->how;
      break;
    }
    case __NR_fork:
    case __NR_vfork:
      // No arguments to copy
      break;
  }
}

#endif
