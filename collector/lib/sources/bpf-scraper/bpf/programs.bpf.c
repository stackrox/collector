// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Red Hat, Inc. */
#include <vmlinux.h>

#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>

#include "common.h"

char _license[] SEC("license") = "GPL";

static const char* get_name(struct btf* btf, long btf_id, const char* fallback) {
  struct btf_type **types, *t;
  unsigned int name_off;
  const char* str;

  if (!btf) {
    return fallback;
  }
  str = btf->strings;
  types = btf->types;
  bpf_probe_read_kernel(&t, sizeof(t), types + btf_id);
  name_off = BPF_CORE_READ(t, name_off);
  if (name_off >= btf->hdr.str_len) {
    return fallback;
  }
  return str + name_off;
}

SEC("iter/bpf_link")
int dump_bpf_link(struct bpf_iter__bpf_link* ctx) {
  struct seq_file* seq = ctx->meta->seq;
  struct bpf_link* link = ctx->link;
  int link_id;

  if (!link) {
    return 0;
  }

  link_id = link->id;
  BPF_SEQ_PRINTF(seq, "%d\n", link_id);
  return 0;
}

SEC("iter/bpf_prog")
int dump_bpf_prog(struct bpf_iter__bpf_prog* ctx) {
  struct seq_file* seq = ctx->meta->seq;
  __u64 seq_num = ctx->meta->seq_num;
  struct bpf_prog* prog = ctx->prog;
  struct bpf_prog_aux* aux;

  if (!prog) {
    return 0;
  }

  struct bpf_prog_result result = {0};

  aux = prog->aux;

  result.id = aux->id;
  bpf_core_read_str(result.name, BPF_STR_MAX, get_name(aux->btf, aux->func_info[0].type_id, aux->name));
  bpf_core_read_str(result.attached, BPF_STR_MAX, aux->attach_func_name);

  bpf_seq_write(seq, &result, sizeof(result));

  return 0;
}
