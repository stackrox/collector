/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */

#include <helpers/interfaces/fixed_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("ksyscall/fchdir")
int BPF_KSYSCALL(sys_enter_fchdir) {
  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, FCHDIR_E_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_FCHDIR_E);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: fd (type: PT_FD) */
  s32 fd = (s32)extract__syscall_argument(ctx, 0);
  ringbuf__store_s64(&ringbuf, (s64)fd);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("kretsyscall/fchdir")
int BPF_KSYSCALL(sys_exit_fchdir, long ret) {
  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, FCHDIR_X_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_FCHDIR_X);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: res (type: PT_ERRNO)*/
  ringbuf__store_s64(&ringbuf, ret);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== EXIT EVENT ===========================*/
