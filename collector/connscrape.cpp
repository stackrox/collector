// Test program for demonstrating connection scraping.

#include <iostream>

#include "EnvVar.h"
#include "ProcfsScraper.h"

using namespace collector;

namespace {

BoolEnvVar scrape_endpoints("SCRAPE_ENDPOINTS", true);

}  // namespace

int main(int argc, char** argv) {
  const char* proc_dir = "/proc";
  const char* tcp_file_path = "/proc/1/net/tcp";

  if (argc > 1) {
    proc_dir = argv[1];
    tcp_file_path = argv[2];
  }

  ConnScraper scraper(proc_dir);
  std::vector<Connection> conns;
  std::vector<ContainerEndpoint> endpoints;

  std::string str_tcp_file_path = tcp_file_path;
  if (!scraper.Scrape(&conns, scrape_endpoints ? &endpoints : nullptr, str_tcp_file_path)) {
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
