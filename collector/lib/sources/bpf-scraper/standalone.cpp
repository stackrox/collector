
#include <iostream>

#include "BPFProgramIterator.h"

using namespace collector::sources;

int main(int argc, char** argv) {
  BPFProgramIterator iterator;

  if (!iterator.Load()) {
    std::cerr << "Failed to load BPF Program Iterator" << std::endl;
    return -1;
  }

  auto programs = iterator.LoadedPrograms();

  for (auto& prog : programs) {
    std::cout << prog << std::endl;
  }

  iterator.Unload();
  return 0;
}
