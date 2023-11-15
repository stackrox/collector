/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */

#include <preamble.h>

#include <helpers/interfaces/fixed_size_event.h>
#include <helpers/interfaces/variable_size_event.h>
#include <sys/syscall.h>

/*=============================== ENTER EVENT ===========================*/

SEC("ksyscall/chdir")
int BPF_KSYSCALL(sys_enter_chdir) {
  if (!preamble(__NR_chdir)) {
    return 0;
  }

  struct ringbuf_struct ringbuf;
  if (!ringbuf__reserve_space(&ringbuf, CHDIR_E_SIZE)) {
    return 0;
  }

  ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_CHDIR_E);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  // Here we have no parameters to collect.

  /*=============================== COLLECT PARAMETERS  ===========================*/

  ringbuf__submit_event(&ringbuf);

  return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("kretsyscall/chdir")
int BPF_KSYSCALL(sys_exit_chdir, long ret) {
  if (!preamble(__NR_chdir)) {
    return 0;
  }

  struct auxiliary_map* auxmap = auxmap__get();
  if (!auxmap) {
    return 0;
  }

  auxmap__preload_event_header(auxmap, PPME_SYSCALL_CHDIR_X);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  /* Parameter 1: res (type: PT_ERRNO) */
  auxmap__store_s64_param(auxmap, ret);

  /* Parameter 2: path (type: PT_CHARBUF) */
  unsigned long path_pointer = extract__syscall_argument(ctx, 0);
  auxmap__store_charbuf_param(auxmap, path_pointer, MAX_PATH, USER);

  /*=============================== COLLECT PARAMETERS  ===========================*/

  auxmap__finalize_event_header(auxmap);

  auxmap__submit_event(auxmap);

  return 0;
}

/*=============================== EXIT EVENT ===========================*/
