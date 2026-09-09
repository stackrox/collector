#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include <libsinsp/sinsp_observer.h>

class sinsp;
class sinsp_threadinfo;

namespace collector::system_inspector {

class ContainerIDCache final : public sinsp_observer {
 public:
  void Initialise(sinsp& inspector);
  void Prune(sinsp& inspector, uint64_t now_us);
  std::string Get(const sinsp_threadinfo& tinfo) const;

  void on_read(sinsp_evt*, int64_t, int64_t, sinsp_fdinfo*, const char*, uint32_t, uint32_t) override {}
  void on_write(sinsp_evt*, int64_t, int64_t, sinsp_fdinfo*, const char*, uint32_t, uint32_t) override {}
  void on_sendfile(sinsp_evt*, int64_t, uint32_t) override {}
  void on_connect(sinsp_evt*, uint8_t*) override {}
  void on_accept(sinsp_evt*, int64_t, uint8_t*, sinsp_fdinfo*) override {}
  void on_file_open(sinsp_evt*, const std::string&, uint32_t) override {}
  void on_error(sinsp_evt*) override {}
  void on_erase_fd(erase_fd_params*) override {}
  void on_socket_shutdown(sinsp_evt*) override {}
  void on_execve(sinsp_evt* evt) override;
  void on_clone(sinsp_evt*, sinsp_threadinfo* newtinfo, int64_t) override;
  void on_bind(sinsp_evt*) override {}
  void on_socket_status_changed(sinsp_evt*) override {}

 private:
  struct Entry {
    uint64_t clone_ts;
    std::string container_id;
  };

  void Cache(const sinsp_threadinfo& tinfo);

  mutable std::mutex mutex_;
  std::unordered_map<int64_t, Entry> entries_;
  uint64_t last_prune_us_ = 0;
};

}  // namespace collector::system_inspector
