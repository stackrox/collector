
#include <helpers/interfaces/attached_programs.h>
#include <helpers/interfaces/syscalls_dispatcher.h>
#include <sys/syscall.h>

struct sys_enter_args {
  __u64 pad;
  long id;
  unsigned long args[6];
};

struct sys_exit_args {
  __u64 pad;
  long id;
  long ret;
};

static int enter_probe(long id, struct sys_enter_args* ctx);
static int exit_probe(long id, struct sys_exit_args* ctx);

#define PROBE_SIGNATURE(event) \
  SEC("tp/" #event)            \
  int BPF_PROG(event)

#define _COLLECTOR_ENTER_PROBE(name, syscall_id)               \
  PROBE_SIGNATURE(sys_enter_##name) {                          \
    struct sys_enter_args* args = (struct sys_enter_args*)ctx; \
    return enter_probe(syscall_id, args);                      \
  }

#define _COLLECTOR_EXIT_PROBE(name, syscall_id)              \
  PROBE_SIGNATURE(sys_exit_##name) {                         \
    struct sys_exit_args* args = (struct sys_exit_args*)ctx; \
    return exit_probe(syscall_id, args);                     \
  }

#define COLLECTOR_PROBE(name, syscall_id) \
  _COLLECTOR_EXIT_PROBE(name, syscall_id) \
  _COLLECTOR_ENTER_PROBE(name, syscall_id)

// COLLECTOR_PROBE(chdir, __NR_chdir);
#ifdef __NR_accept
// COLLECTOR_PROBE(accept, __NR_accept);
#endif
// COLLECTOR_PROBE(accept4, __NR_accept4);
// COLLECTOR_PROBE(clone, __NR_clone);
// COLLECTOR_PROBE(close, __NR_close);
// COLLECTOR_PROBE(connect, __NR_connect);
// COLLECTOR_PROBE(execve, __NR_execve);
// COLLECTOR_PROBE(getsockopt, __NR_getsockopt);
// COLLECTOR_PROBE(setresgid, __NR_setresgid);
// COLLECTOR_PROBE(setresuid, __NR_setresuid);
// COLLECTOR_PROBE(setgid, __NR_setgid);
// COLLECTOR_PROBE(setuid, __NR_setuid);
// COLLECTOR_PROBE(shutdown, __NR_shutdown);
// COLLECTOR_PROBE(socket, __NR_socket);
// #ifdef CAPTURE_SOCKETCALL
// // The socketcall handling in driver/bpf/plumbing_helpers.h will filter
// // socket calls based on those mentioned here.  Therefore, updates to
// // socket calls needs to be synchronized.
// COLLECTOR_PROBE(socketcall, __NR_socketcall)
// #endif
// COLLECTOR_PROBE(fchdir, __NR_fchdir);
// COLLECTOR_PROBE(fork, __NR_fork);
// COLLECTOR_PROBE(vfork, __NR_vfork);

int enter_probe(long id, struct sys_enter_args* ctx) {
  if (!syscalls_dispatcher__64bit_interesting_syscall(id)) {
    return 0;
  }

  if (sampling_logic(id, SYSCALL)) {
    return 0;
  }

  if (syscalls_dispatcher__check_32bit_syscalls()) {
    return 0;
  }

  bpf_tail_call(ctx, &syscall_enter_tail_table, id);
  return 0;
}

int exit_probe(long id, struct sys_exit_args* ctx) {
  u32 syscall_id = ctx->ret;

#ifdef CAPTURE_SOCKETCALL
  /* we convert it here in this way the syscall will be treated exactly as the original one */
  if (syscall_id == __NR_socketcall) {
    syscall_id = convert_network_syscalls(regs);
  }
#endif

  /* The `syscall-id` can refer to both 64-bit and 32-bit architectures.
   * Right now we filter only 64-bit syscalls, all the 32-bit syscalls
   * will be dropped with `syscalls_dispatcher__check_32bit_syscalls`.
   *
   * If the syscall is not interesting we drop it.
   */
  if (!syscalls_dispatcher__64bit_interesting_syscall(syscall_id)) {
    return 0;
  }

  if (sampling_logic(syscall_id, SYSCALL)) {
    return 0;
  }

  if (syscalls_dispatcher__check_32bit_syscalls()) {
    return 0;
  }

  bpf_tail_call(ctx, &syscall_exit_tail_table, syscall_id);

  return 0;
}
