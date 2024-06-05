#ifndef _COLLECTOR_SOURCES_TYPES_H
#define _COLLECTOR_SOURCES_TYPES_H

#include <cstdint>
#include <string>

namespace collector {
namespace sources {

enum EventKind {
  Process,
  Endpoint,
  Connection,
};

class IEvent {
 public:
  virtual EventKind Kind() = 0;
};

class IProcess : public IEvent {
 public:
  virtual ~IProcess() {}

  EventKind Kind() override {
    return EventKind::Process;
  }

  virtual std::uint64_t pid() const = 0;
  virtual std::string container_id() const = 0;
  virtual std::string comm() const = 0;
  virtual std::string exe() const = 0;
  virtual std::string exe_path() const = 0;
  virtual std::string args() const = 0;

  virtual bool operator==(IProcess& other) {
    return pid() == other.pid();
  }
};

class IEndpoint : public IEvent {
 public:
  virtual ~IEndpoint() {}

  EventKind Kind() override {
    return EventKind::Endpoint;
  }
};

class IConnection : public IEvent {
 public:
  virtual ~IConnection() {}

  EventKind Kind() override {
    return EventKind::Connection;
  }
};

}  // namespace sources
}  // namespace collector

#endif
