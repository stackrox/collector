/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */
#include <preamble.h>

#include <helpers/interfaces/fixed_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("ksyscall/socket")
int BPF_KSYSCALL(sys_enter_socket) {
  if (!preamble(__NR_socket)) {
    return 0;
  }

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SOCKET_E_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SOCKET_SOCKET_E);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Collect parameters at the beginning so we can easily manage socketcalls */
  unsigned long args[3];
  extract__network_args(args, 3, ctx);

  /* Parameter 1: domain (type: PT_ENUMFLAGS32) */
  /* why to send 32 bits if we need only 8 bits? */
  u8 domain = (u8)args[0];
  ringbuf__store_u32(&ringbuf, (u32)socket_family_to_scap(domain));

  /* Parameter 2: type (type: PT_UINT32) */
  /* this should be an int, not a uint32 */
  u32 type = (u32)args[1];
  ringbuf__store_u32(&ringbuf, type);

  /* Parameter 3: proto (type: PT_UINT32) */
  /* this should be an int, not a uint32 */
  u32 proto = (u32)args[2];
  ringbuf__store_u32(&ringbuf, proto);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("kretsyscall/socket")
int BPF_KSYSCALL(sys_exit_socket, long ret) {
  if (!preamble(__NR_socket)) {
    return 0;
  }

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, SOCKET_X_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SOCKET_SOCKET_X);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: res (type: PT_ERRNO)*/
  ringbuf__store_s64(&ringbuf, ret);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== EXIT EVENT ===========================*/
