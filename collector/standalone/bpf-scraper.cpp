
#include <iostream>

#include "Logging.h"
#include "Utility.h"
#include "sources/bpf-scraper/BPFProgramIterator.h"

using namespace collector::sources;

int main(int argc, char** argv) {
  BPFProgramIterator iterator;
  collector::logging::SetLogLevel(collector::logging::LogLevel::DEBUG);

  if (!iterator.Load()) {
    std::cerr << "Failed to load BPF Program Iterator" << std::endl;
    return -1;
  }

  auto programs = iterator.LoadedPrograms();

  for (auto* prog : programs) {
    collector::LogProtobufMessage(*prog);
  }

  iterator.Unload();
  return 0;
}
