/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */
#include <preamble.h>

#include <helpers/interfaces/fixed_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("ksyscall/setgid")
int BPF_KSYSCALL(sys_enter_setgid) {
  if (!preamble(__NR_setgid)) {
    return 0;
  }

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SETGID_E_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_SETGID_E);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: gid (type: PT_GID) */
  u32 gid = (u32)extract__syscall_argument(ctx, 0);
  bpf_printk("gid: %d", gid);
  ringbuf__store_u32(&ringbuf, gid);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("kretsyscall/setgid")
int BPF_KSYSCALL(sys_exit_setgid, long ret) {
  if (!preamble(__NR_setgid)) {
    return 0;
  }

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SETGID_X_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_SETGID_X);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: res (type: PT_ERRNO) */
  ringbuf__store_s64(&ringbuf, ret);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== EXIT EVENT ===========================*/
