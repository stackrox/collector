#include "BPFProgramIterator.h"

#include <unistd.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "bpf/common.h"
#include "program_iter.skel.h"

namespace collector::sources {

static struct program_iter_bpf* skeleton_;

bool BPFProgramIterator::Load() {
  skeleton_ = program_iter_bpf__open_and_load();
  if (!skeleton_) {
    return false;
  }

  if (program_iter_bpf__attach(skeleton_) != 0) {
    return false;
  }

  return true;
}

void BPFProgramIterator::Unload() {
  program_iter_bpf__destroy(skeleton_);
}

std::vector<sensor::SignalStreamMessage*> BPFProgramIterator::LoadedPrograms() {
  int iter_fd = bpf_iter_create(bpf_link__fd(skeleton_->links.dump_bpf_prog));
  if (iter_fd < 0) {
    return {};
  }

  std::vector<sensor::SignalStreamMessage*> messages;
  struct bpf_prog_result result;

  while (true) {
    int ret = read(iter_fd, &result, sizeof(struct bpf_prog_result));
    if (ret < 0) {
      if (errno == EAGAIN) {
        continue;
      }
      break;
    }

    if (ret == 0) {
      break;
    }

    auto message = formatter_.ToProtoMessage(&result);
    if (message == nullptr) {
      continue;
    }

    messages.push_back(message);
  }

  return messages;
}

}  // namespace collector::sources
