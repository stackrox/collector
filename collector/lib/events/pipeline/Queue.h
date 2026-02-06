#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>

namespace collector::pipeline {
template <typename T>
class Queue {
 public:
  Queue() {}
  ~Queue() {}

  bool empty() {
    auto lock = read_lock();
    return inner_.empty();
  }

  size_t size() {
    auto lock = read_lock();
    return inner_.size();
  }

  void push(const T& elem) {
    {
      auto lock = write_lock();
      auto e = elem;
      inner_.push(std::move(e));
    }
    state_changed_.notify_one();
  }

  void push(T&& elem) {
    {
      auto lock = write_lock();
      inner_.push(elem);
    }
    state_changed_.notify_one();
  }

  std::optional<T> pop(std::chrono::milliseconds wait_max = std::chrono::milliseconds(10)) {
    auto lock = write_lock();
    if (inner_.empty()) {
      auto pred = [this]() {
        return !inner_.empty();
      };

      if (!state_changed_.wait_for(lock, wait_max, pred)) {
        return std::nullopt;
      }
    }
    T data = inner_.front();
    inner_.pop();
    return {data};
  }

  std::shared_lock<std::shared_mutex> read_lock() const {
    return std::shared_lock(mx_);
  }

  std::unique_lock<std::shared_mutex> write_lock() const {
    return std::unique_lock(mx_);
  }

 private:
  std::queue<T> inner_;

  mutable std::shared_mutex mx_;
  mutable std::condition_variable_any state_changed_;
};
}  // namespace collector::pipeline
