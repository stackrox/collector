static int enter_probe(long id, struct sys_enter_args* ctx);
static int exit_probe(long id, struct sys_exit_args* ctx);

#define PROBE_SIGNATURE(prefix, event, type) \
    SEC("tp_btf/" #event) BPF_PROG(sys_enter_##event(struct type* ctx)

#define _COLLECTOR_ENTER_PROBE(name, syscall_id)                   \
  PROBE_SIGNATURE("syscalls/", sys_enter_##name, sys_enter_args) { \
    return enter_probe(syscall_id, ctx);                           \
  }

#define _COLLECTOR_EXIT_PROBE(name, syscall_id)                  \
  PROBE_SIGNATURE("syscalls/", sys_exit_##name, sys_exit_args) { \
    return exit_probe(syscall_id, ctx);                          \
  }

#define COLLECTOR_PROBE(name, syscall_id) \
  _COLLECTOR_EXIT_PROBE(name, syscall_id) \
  _COLLECTOR_ENTER_PROBE(name, syscall_id)

COLLECTOR_PROBE(chdir, __NR_chdir);

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
  u32 syscall_id = ctx->retval;

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
