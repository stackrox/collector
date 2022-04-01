

// Test program for demonstrating connection scraping.

#include <iostream>

#include "ConnScraper.h"
#include "EnvVar.h"

using namespace collector;

namespace {

BoolEnvVar scrape_endpoints("SCRAPE_ENDPOINTS", false);

}  // namespace

int main(int argc, char** argv) {
  const char* proc_dir = "/proc";

  if (argc > 1) {
    proc_dir = argv[1];
  }

  ConnScraper scraper(proc_dir);
  std::vector<Connection> conns;
  std::vector<ContainerEndpoint> endpoints;

  if (!scraper.Scrape(&conns, scrape_endpoints ? &endpoints : nullptr)) {
    std::cerr << "Failed to scrape :(" << std::endl;
    return 1;
  }

  if (scrape_endpoints) {
    std::cout << "Connections:" << std::endl;
  }
  for (const auto& conn : conns) {
    std::cout << " " << conn << std::endl;
  }

  if (scrape_endpoints) {
    std::cout << std::endl
              << "Endpoints:" << std::endl;
    for (const auto& ep : endpoints) {
      std::cout << " " << ep << std::endl;
    }
  }

  return 0;
}
