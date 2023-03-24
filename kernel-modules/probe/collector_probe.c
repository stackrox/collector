#include "quirks.h"

// This definition is set when the Kernel version is newer than
// 4.17.0. It affects the way nearly everything in the falco probe
// is processed, and is a bit of a misnomer. It might be more appropriately
// called "USING_RAW_TRACEPOINTS"
//
// Since we are *not* using raw tracepoints, we unset this definition before
// including any other files, to ensure that the behaviour and structures are
// as expected for the kinds of tracepoints we are using.
#ifdef BPF_SUPPORTS_RAW_TRACEPOINTS
#  undef BPF_SUPPORTS_RAW_TRACEPOINTS
#endif

// this if statement relies on short circuiting to simplify the definition
// of the tracepoints. i.e. RHEL_RELEASE_VERSION will not be defined unless
// RHEL_RELEASE_CODE is defined.
// This enables the direct-attached BPF probes to specific syscalls.
// Note that this needs to be defined before including Falco libs includes
// as there are syscall-specific vs. general syscall enter/exit format/structure
// alignments necessary in Falco.
#if !defined(RHEL_RELEASE_CODE) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 0)
#  define USE_COLLECTOR_CUSTOM_PROBES
#endif

#include <generated/utsrelease.h>
#include <linux/sched.h>
#include <sys/syscall.h>
#include <uapi/linux/bpf.h>

// Unfortunately include order is important here, so turn clang-format
// off to avoid reordering as part of reformatting.
// clang-format off
#include "../driver_config.h"
#include "../ppm_events_public.h"
#include "../ppm_version.h"
#include "bpf_helpers.h"
#include "types.h"
#include "maps.h"
#include "plumbing_helpers.h"
#include "ring_helpers.h"
#include "filler_helpers.h"
#include "fillers.h"
#include "builtins.h"
// clang-format on

static __always_inline int enter_probe(long id, struct sys_enter_args* ctx);
static __always_inline int exit_probe(long id, struct sys_exit_args* ctx);

// defines the maximum number of args to copy from the context
// to the stack. Matches up to the structure defined in types.h
#define NUM_SYS_ENTER_ARGS 6

// this can be passed to the enter or exit functions as the ID value
// to pull the ID from the context. This is specifically useful when
// we need to use the sys_enter/sys_exit tracepoints.
#define LOOKUP_SYSCALL_ID -1

/**
 * @brief Encapsulates the section definition and unified function signature. This is used
 *        to create a new section for each eBPF program, which are then processed by
 *        falco to attach those programs to the specified tracepoints (based on the section name)
 *
 *        e.g. section tracepoint/syscalls/sys_enter_accept will contain the sys_enter_accept
 *             program, and will be attached to tracepoint/syscalls/sys_enter_accept
 *
 * @param prefix the kind of tracepoint to attach to. e.g. "syscall/" or "sched/"
 * @param event the event to attach to. e.g. sys_enter_accept
 * @param type the type name for the context argument
 */
#define PROBE_SIGNATURE(prefix, event, type) \
  __bpf_section("tracepoint/" prefix #event) int bpf_##event(struct type* ctx)

/**
 * @brief Defines the syscall-specific enter eBPF program.
 *
 * @param name the syscall name. e.g. accept, chdir, execve
 * @param syscall_id the ID number for the syscall. e.g. __NR_accept
 */
#define _COLLECTOR_ENTER_PROBE(name, syscall_id)                   \
  PROBE_SIGNATURE("syscalls/", sys_enter_##name, sys_enter_args) { \
    return enter_probe(syscall_id, ctx);                           \
  }

/**
 * @brief Defines the syscall-specific exit eBPF program.
 *
 * @param name the syscall name. e.g. accept, chdir, execve
 * @param syscall_id the ID number for the syscall. e.g. __NR_accept
 */
#define _COLLECTOR_EXIT_PROBE(name, syscall_id)                  \
  PROBE_SIGNATURE("syscalls/", sys_exit_##name, sys_exit_args) { \
    return exit_probe(syscall_id, ctx);                          \
  }

/**
 * @brief Defines the catch-all sys_enter program for older platforms
 *        that do not support all the tracepoints we need.
 */
#define _COLLECTOR_SYS_ENTER_PROBE                              \
  PROBE_SIGNATURE("raw_syscalls/", sys_enter, sys_enter_args) { \
    return enter_probe(LOOKUP_SYSCALL_ID, ctx);                 \
  }

/**
 * @brief Defines the catch-all sys_exit program for older platforms
 *        that do not support all the tracepoints we need.
 */
#define _COLLECTOR_SYS_EXIT_PROBE                             \
  PROBE_SIGNATURE("raw_syscalls/", sys_exit, sys_exit_args) { \
    return exit_probe(LOOKUP_SYSCALL_ID, ctx);                \
  }

/**
 * @brief Brings together the enter and exit definitions, to define all programs
 *        for a given syscall.
 *
 * @param name the syscall name. e.g. accept, chdir, execve
 * @param syscall_id the ID number for the syscall. e.g. __NR_accept
 */
#define COLLECTOR_PROBE(name, syscall_id)  \
  _COLLECTOR_ENTER_PROBE(name, syscall_id) \
  _COLLECTOR_EXIT_PROBE(name, syscall_id)

/**
 * @brief brings together the sys_enter and sys_exit probes for legacy platforms
 *        that do not support all the necessary tracepoints.
 */
#define COLLECTOR_LEGACY_PROBE() \
  _COLLECTOR_SYS_ENTER_PROBE;    \
  _COLLECTOR_SYS_EXIT_PROBE

// this if statement relies on short circuiting to simplify the definition
// of the tracepoints. i.e. RHEL_RELEASE_VERSION will not be defined unless
// RHEL_RELEASE_CODE is defined.
#ifdef USE_COLLECTOR_CUSTOM_PROBES

COLLECTOR_PROBE(chdir, __NR_chdir);
#  ifdef __NR_accept
COLLECTOR_PROBE(accept, __NR_accept);
#  endif
COLLECTOR_PROBE(accept4, __NR_accept4);
COLLECTOR_PROBE(clone, __NR_clone);
COLLECTOR_PROBE(close, __NR_close);
COLLECTOR_PROBE(connect, __NR_connect);
COLLECTOR_PROBE(execve, __NR_execve);
COLLECTOR_PROBE(setresgid, __NR_setresgid);
COLLECTOR_PROBE(setresuid, __NR_setresuid);
COLLECTOR_PROBE(setgid, __NR_setgid);
COLLECTOR_PROBE(setuid, __NR_setuid);
COLLECTOR_PROBE(shutdown, __NR_shutdown);
COLLECTOR_PROBE(socket, __NR_socket);
#  ifdef CAPTURE_SOCKETCALL
// The socketcall handling in driver/bpf/plumbing_helpers.h will filter
// socket calls based on those mentioned here.  Therefore, updates to
// socket calls needs to be synchronized.
COLLECTOR_PROBE(socketcall, __NR_socketcall)
#  endif
COLLECTOR_PROBE(fchdir, __NR_fchdir);
COLLECTOR_PROBE(fork, __NR_fork);
COLLECTOR_PROBE(vfork, __NR_vfork);

#else

// Unfortunately RHEL-7 does not have the necessary tracepoints that we require
// for the collector probe (specifically clone, execve, fork, and vfork)
// so we just build the falco catch-all probe for this platform
COLLECTOR_LEGACY_PROBE();

#endif

/**
 * @brief program for handling sched_process_fork events. As the name suggests
 *        they occur when a process forks, and we get information here about
 *        the parent and the child. This program is purely responsible for
 *        stashing args for the child (which are a copy of the parent's)
 *
 *        These stashed args are used in subsequent process events where
 *        these args are not available (e.g. fork).
 */
PROBE_SIGNATURE("sched/", sched_process_fork, sched_process_fork_args) {
  enum ppm_event_type evt_type;
  struct sys_stash_args args;
  unsigned long* argsp;

  // using the "private" version of this function so we can
  // provide a pid.
  argsp = __unstash_args(ctx->parent_pid);
  if (argsp == NULL) {
    return 0;
  }

  memcpy(&args, argsp, sizeof(args));

  __stash_args(ctx->child_pid, args.args);
  return 0;
}

/**
 * @brief program for handling sched_process_exit events. As the name suggests
 *        they occur when a process exits. Minimal processing is performed here
 *        instead, we defer to the appropriate filler.
 */
PROBE_SIGNATURE("sched/", sched_process_exit, sched_process_exit_args) {
  enum ppm_event_type evt_type = PPME_PROCEXIT_1_E;
  struct task_struct* task = NULL;
  unsigned int flags = 0;

  task = (struct task_struct*)bpf_get_current_task();

  flags = _READ(task->flags);
  if ((flags & PF_KTHREAD) != 0) {
    // we only want to process userspace threads.
    return 0;
  }

  call_filler(ctx, ctx, evt_type, UF_NEVER_DROP);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////
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
  uid_t uid;
};

struct sys_enter_setgid_args {
  __u64 pad;
  int __syscall_nr;
  gid_t gid;
};

struct sys_enter_setresuid_args {
  __u64 pad;
  int __syscall_nr;
  uid_t ruid;
  uid_t euid;
  uid_t suid;
};

struct sys_enter_setresgid_args {
  __u64 pad;
  int __syscall_nr;
  gid_t rgid;
  gid_t egid;
  gid_t sgid;
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
////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Generic sys_enter_* program for any system call. It is responsible for
 *        Verifying userspace settings and early processing of the syscall event.
 *
 *        The function will exit 0 (zero) regardless of outcome.
 *
 * @param id the syscall id
 * @param ctx the context pointer as provided by the kernel
 *
 * @return 0 (regardless of outcomes)
 */
static __always_inline int enter_probe(long id, struct sys_enter_args* ctx) {
  const struct syscall_evt_pair* sc_evt = NULL;
  enum ppm_event_type evt_type = PPME_GENERIC_E;
  int drop_flags = UF_ALWAYS_DROP;
  struct sys_enter_args stack_ctx = {.id = id};
  long mapped_id = id;

  if (bpf_in_ia32_syscall()) {
    return 0;
  }

  if (id == LOOKUP_SYSCALL_ID) {
    // this is to support sys_enter and sys_exit probes for older (RHEL 7)
    // platforms. Just get the id from the context for this scenario.
    id = bpf_syscall_get_nr(ctx);
    stack_ctx.id = id;
  }

#if defined(CAPTURE_SOCKETCALL)
  if(id == __NR_socketcall)
  {
	  mapped_id = convert_network_syscalls(ctx);
  }
#endif

  sc_evt = get_syscall_info(mapped_id);
  if (sc_evt == NULL || (sc_evt->flags & UF_USED) == 0) {
    return 0;
  } else {
    evt_type = sc_evt->enter_event_type;
    drop_flags = sc_evt->flags;
  }

  /* Syscall tracepoints follow their own format (=structure) of arguments.
   * Copying all arguments, e.g. with
   *   memcpy(stack_ctx.args, _READ(ctx->args), sizeof(unsigned long) * NUM_SYS_ENTER_ARGS);
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
      stack_ctx.args[0] = (unsigned long)accept4_args->fd;
      stack_ctx.args[1] = (unsigned long)accept4_args->upeer_sockaddr;
      stack_ctx.args[2] = (unsigned long)accept4_args->upeer_len;
      break;
    }
#endif
    case __NR_accept4: {
      struct sys_enter_accept4_args* accept4_args = (struct sys_enter_accept4_args*)ctx;
      stack_ctx.args[0] = (unsigned long)accept4_args->fd;
      stack_ctx.args[1] = (unsigned long)accept4_args->upeer_sockaddr;
      stack_ctx.args[2] = (unsigned long)accept4_args->upeer_len;
      stack_ctx.args[3] = (unsigned long)accept4_args->flags;
      break;
    }
    case __NR_connect: {
      struct sys_enter_connect_args* connect_args = (struct sys_enter_connect_args*)ctx;
      stack_ctx.args[0] = (unsigned long)connect_args->fd;
      stack_ctx.args[1] = (unsigned long)connect_args->uservaddr;
      stack_ctx.args[2] = (unsigned long)connect_args->addrlen;
      break;
    }
    case __NR_chdir: {
      struct sys_enter_chdir_args* chdir_args = (struct sys_enter_chdir_args*)ctx;
      stack_ctx.args[0] = (unsigned long)chdir_args->filename;
      break;
    }
    case __NR_fchdir: {
      struct sys_enter_fchdir_args* fchdir_args = (struct sys_enter_fchdir_args*)ctx;
      stack_ctx.args[0] = (unsigned long)fchdir_args->fd;
      break;
    }
    case __NR_clone: {
      struct sys_enter_clone_args* clone_args = (struct sys_enter_clone_args*)ctx;
#ifdef __s390x__
      stack_ctx.args[0] = (unsigned long)clone_args->newsp;
      stack_ctx.args[1] = (unsigned long)clone_args->clone_flags;
      stack_ctx.args[2] = (unsigned long)clone_args->parent_tidptr;
      stack_ctx.args[3] = (unsigned long)clone_args->child_tidptr;
      stack_ctx.args[4] = (unsigned long)clone_args->tls;
#elif __aarch64__
      stack_ctx.args[0] = (unsigned long)clone_args->clone_flags;
      stack_ctx.args[1] = (unsigned long)clone_args->newsp;
      stack_ctx.args[2] = (unsigned long)clone_args->parent_tidptr;
      stack_ctx.args[3] = (unsigned long)clone_args->tls;
      stack_ctx.args[4] = (unsigned long)clone_args->child_tidptr;
#else
      stack_ctx.args[0] = (unsigned long)clone_args->clone_flags;
      stack_ctx.args[1] = (unsigned long)clone_args->newsp;
      stack_ctx.args[2] = (unsigned long)clone_args->parent_tidptr;
      stack_ctx.args[3] = (unsigned long)clone_args->child_tidptr;
      stack_ctx.args[4] = (unsigned long)clone_args->tls;
#endif
      break;
    }
    case __NR_execve: {
      struct sys_enter_execve_args* execve_args = (struct sys_enter_execve_args*)ctx;
      stack_ctx.args[0] = (unsigned long)execve_args->filename;
      stack_ctx.args[1] = (unsigned long)execve_args->argv;
      stack_ctx.args[2] = (unsigned long)execve_args->envp;
      break;
    }
    case __NR_close: {
      struct sys_enter_close_args* close_args = (struct sys_enter_close_args*)ctx;
      stack_ctx.args[0] = (unsigned long)close_args->fd;
      break;
    }
    case __NR_setuid: {
      struct sys_enter_setuid_args* setuid_args = (struct sys_enter_setuid_args*)ctx;
      stack_ctx.args[0] = (unsigned long)setuid_args->uid;
      break;
    }
    case __NR_setgid: {
      struct sys_enter_setgid_args* setgid_args = (struct sys_enter_setgid_args*)ctx;
      stack_ctx.args[0] = (unsigned long)setgid_args->gid;
      break;
    }
    case __NR_setresgid: {
      struct sys_enter_setresgid_args* setresgid_args = (struct sys_enter_setresgid_args*)ctx;
      stack_ctx.args[0] = (unsigned long)setresgid_args->rgid;
      stack_ctx.args[1] = (unsigned long)setresgid_args->egid;
      stack_ctx.args[2] = (unsigned long)setresgid_args->sgid;
      break;
    }
    case __NR_setresuid: {
      struct sys_enter_setresuid_args* setresuid_args = (struct sys_enter_setresuid_args*)ctx;
      stack_ctx.args[0] = (unsigned long)setresuid_args->ruid;
      stack_ctx.args[1] = (unsigned long)setresuid_args->euid;
      stack_ctx.args[2] = (unsigned long)setresuid_args->suid;
      break;
    }
    case __NR_socket: {
      struct sys_enter_socket_args* socket_args = (struct sys_enter_socket_args*)ctx;
      stack_ctx.args[0] = (unsigned long)socket_args->family;
      stack_ctx.args[1] = (unsigned long)socket_args->type;
      stack_ctx.args[2] = (unsigned long)socket_args->protocol;
      break;
    }
#ifdef CAPTURE_SOCKETCALL
    case __NR_socketcall: {
      struct sys_enter_socketcall_args* socketcall_args = (struct sys_enter_socketcall_args*)ctx;
      unsigned long socketcall_id = (unsigned long)socketcall_args->call;

      stack_ctx.args[0] = socketcall_id;
      stack_ctx.args[1] = (unsigned long)socketcall_args->args;

      switch (socketcall_id) {
        case SYS_ACCEPT:
        case SYS_ACCEPT4:
        case SYS_CONNECT:
        case SYS_SHUTDOWN:
        case SYS_SOCKET:
          break;
        default:
          // Filter out any other socket calls
          return 0;
      }
      break;
    }
#endif
    case __NR_shutdown: {
      struct sys_enter_shutdown_args* shutdown_args = (struct sys_enter_shutdown_args*)ctx;
      stack_ctx.args[0] = (unsigned long)shutdown_args->fd;
      stack_ctx.args[1] = (unsigned long)shutdown_args->how;
      break;
    }
    case __NR_fork:
    case __NR_vfork:
      // No arguments to copy
      break;
  }

  // stashing the args will copy it into a BPF map for later
  // processing. This is a required step for the enter probe,
  // and these args are subsequently pulled out of the map and
  // written to the ring buffer.
  //
  // The args pointer must exist for the lifetime of this event, so we
  // must not use _READ(stack_ctx.args) here.
  if (stash_args(stack_ctx.args)) {
    return 0;
  }

  // the fillers contain syscall specific processing logic, so we simply
  // call into those and let the rest of falco deal with the event.
  //
  // It also handles the stack context problem, so we can pass both
  // pointers through without issue.
  call_filler(ctx, &stack_ctx, evt_type, drop_flags);
  return 0;
}

/**
 * @brief Generic sys_exit_* program for any system call. It is responsible for
 *        Verifying userspace settings and early processing of the syscall event.
 *
 *        The function will exit 0 (zero) regardless of outcome.
 *
 * @param id the syscall id
 * @param ctx the eBPF context as provided by the kernel
 *
 * @return 0 (regardless of outcomes)
 */
static __always_inline int exit_probe(long id, struct sys_exit_args* ctx) {
  const struct syscall_evt_pair* sc_evt = NULL;
  enum ppm_event_type evt_type = PPME_GENERIC_X;
  int drop_flags = UF_ALWAYS_DROP;
  long mapped_id = id;

  if (bpf_in_ia32_syscall()) {
    return 0;
  }

  if (id == LOOKUP_SYSCALL_ID) {
    // this is to support sys_enter and sys_exit probes for legacy (RHEL 7)
    // platforms. Just get the id from the context for this scenario.
    id = bpf_syscall_get_nr(ctx);
  }

#if defined(CAPTURE_SOCKETCALL)
  if(id == __NR_socketcall)
  {
	  mapped_id = convert_network_syscalls(ctx);
  }
#endif

  sc_evt = get_syscall_info(mapped_id);
  if (sc_evt == NULL || (sc_evt->flags & UF_USED) == 0) {
    return 0;
  } else {
    evt_type = sc_evt->exit_event_type;
    drop_flags = sc_evt->flags;
  }

  // the fillers contain syscall specific processing logic, so we simply
  // call into those and let the rest of falco deal with the event.
  call_filler(ctx, ctx, evt_type, drop_flags);
  return 0;
}

char kernel_ver[] __bpf_section("kernel_version") = UTS_RELEASE;

char __license[] __bpf_section("license") = "GPL";

char probe_ver[] __bpf_section("probe_version") = DRIVER_VERSION;

char probe_commit[] __bpf_section("build_commit") = DRIVER_COMMIT;

uint64_t probe_api_ver __bpf_section("api_version") = PPM_API_CURRENT_VERSION;

uint64_t probe_schema_ver __bpf_section("schema_version") = PPM_SCHEMA_CURRENT_VERSION;
