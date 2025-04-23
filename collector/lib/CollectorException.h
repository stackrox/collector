#pragma once

#include <exception>
#include <string>
#include <utility>

namespace collector {

class CollectorException : public std::exception {
 public:
  CollectorException() : CollectorException("Unknown collector exception") {}
  CollectorException(std::string message) : message_(std::move(message)) {}

  const char* what() const noexcept override { return message_.c_str(); }

 private:
  std::string message_;
};

}  // namespace collector
