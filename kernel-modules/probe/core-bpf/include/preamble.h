#pragma once

#include <helpers/interfaces/attached_programs.h>
#include <helpers/interfaces/syscalls_dispatcher.h>
#include <sys/syscall.h>

static __always_inline int preamble(long syscall_id) {
  if (!syscalls_dispatcher__64bit_interesting_syscall(syscall_id)) {
    return 0;
  }

  if (sampling_logic(syscall_id, SYSCALL)) {
    return 0;
  }

  if (syscalls_dispatcher__check_32bit_syscalls()) {
    return 0;
  }

  return 1;
}
