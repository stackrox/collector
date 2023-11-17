/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */
#include <preamble.h>

#include <helpers/interfaces/fixed_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("ksyscall/setresuid")
int BPF_KSYSCALL(sys_enter_setresuid) {
  if (!preamble(__NR_setresuid)) {
    return 0;
  }

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SETRESUID_E_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_SETRESUID_E);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: ruid (type: PT_GID) */
  uid_t ruid = (u32)extract__syscall_argument(ctx, 0);
  ringbuf__store_u32(&ringbuf, ruid);

  /* Parameter 2: euid (type: PT_GID) */
  uid_t euid = (u32)extract__syscall_argument(ctx, 1);
  ringbuf__store_u32(&ringbuf, euid);

  /* Parameter 3: suid (type: PT_GID) */
  uid_t suid = (u32)extract__syscall_argument(ctx, 2);
  ringbuf__store_u32(&ringbuf, suid);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("kretsyscall/setresuid")
int BPF_KSYSCALL(sys_exit_setresuid, long ret) {
  if (!preamble(__NR_setresuid)) {
    return 0;
  }

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SETRESUID_X_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_SETRESUID_X);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: res (type: PT_ERRNO)*/
  ringbuf__store_s64(&ringbuf, ret);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== EXIT EVENT ===========================*/
