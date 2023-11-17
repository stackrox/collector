/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */

#include <preamble.h>

#include <helpers/interfaces/fixed_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("ksyscall/close")
int BPF_KSYSCALL(sys_enter_close) {
  if (!preamble(__NR_close)) {
    return 0;
  }

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, CLOSE_E_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_CLOSE_E);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: fd (type: PT_FD)*/
  s32 fd = (s32)extract__syscall_argument(ctx, 0);
  ringbuf__store_s64(&ringbuf, (s64)fd);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("kretsyscall/close")
int BPF_KSYSCALL(sys_exit_close, long ret) {
  if (!preamble(__NR_close)) {
    return 0;
  }

  bpf_printk("ret: %d", ret);

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, CLOSE_X_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_CLOSE_X);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: res (type: PT_ERRNO)*/
  ringbuf__store_s64(&ringbuf, ret);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== EXIT EVENT ===========================*/
