#ifndef _COLLECTOR_BPF_PROGRAM_ITERATOR_
#define _COLLECTOR_BPF_PROGRAM_ITERATOR_

#include <vector>

#include "internalapi/sensor/signal_iservice.pb.h"

#include "BPFSignalFormatter.h"

namespace collector::sources {
class BPFProgramIterator {
 public:
  bool Load();
  void Unload();

  std::vector<sensor::SignalStreamMessage*> LoadedPrograms();

 private:
  BPFSignalFormatter formatter_;
};
};  // namespace collector::sources

#endif
