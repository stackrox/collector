/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */

#include <helpers/interfaces/fixed_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("ksyscall/setuid")
int BPF_KSYSCALL(sys_enter_setuid) {
  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SETUID_E_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_SETUID_E);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: uid (type: PT_UID) */
  uid_t uid = (u32)extract__syscall_argument(ctx, 0);
  ringbuf__store_u32(&ringbuf, uid);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("kretsyscall/setuid")
int BPF_KSYSCALL(sys_exit_setuid, long ret) {
  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SETUID_X_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_SETUID_X);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: res (type: PT_ERRNO) */
  ringbuf__store_s64(&ringbuf, ret);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== EXIT EVENT ===========================*/
