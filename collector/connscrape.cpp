#include <iostream>

#include "ConnScraper.h"

using namespace collector;

int main(int argc, char** argv) {
  const char* proc_dir = "/proc";

  if (argc > 1) {
    proc_dir = argv[1];
  }

  ConnScraper scraper(proc_dir);
  std::vector<Connection> conns;

  if (!scraper.Scrape(&conns)) {
    std::cerr << "Failed to scrape :(" << std::endl;
    return 1;
  }

  for (const auto& conn : conns) {
    std::cout << conn << std::endl;
  }

  return 0;
}
