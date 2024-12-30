#ifndef __SCRAPER_BPF_ITER_H
#define __SCRAPER_BPF_ITER_H

#define BPF_STR_MAX 32

struct bpf_prog_result {
  unsigned int id;
  char name[BPF_STR_MAX];
  char attached[BPF_STR_MAX];
};

#endif
