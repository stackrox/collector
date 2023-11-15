/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */

#include <helpers/interfaces/fixed_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("ksyscall/setresgid")
int BPF_KSYSCALL(sys_enter_setresgid) {
  struct sys_enter_args* enter = (struct sys_enter_args*)ctx;
  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SETRESGID_E_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_SETRESGID_E);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: rgid (type: PT_GID) */
  gid_t rgid = (u32)extract__syscall_argument(ctx, 0);
  ringbuf__store_u32(&ringbuf, rgid);

  /* Parameter 2: egid (type: PT_GID) */
  gid_t egid = (u32)extract__syscall_argument(ctx, 1);
  ringbuf__store_u32(&ringbuf, egid);

  /* Parameter 3: sgid (type: PT_GID) */
  gid_t sgid = (u32)extract__syscall_argument(ctx, 2);
  ringbuf__store_u32(&ringbuf, sgid);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("kretsyscall/setresgid")
int BPF_KSYSCALL(sys_exit_setresgid, long ret) {
  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SETRESGID_X_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_SETRESGID_X);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: res (type: PT_ERRNO)*/
  ringbuf__store_s64(&ringbuf, ret);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== EXIT EVENT ===========================*/
