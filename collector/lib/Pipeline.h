#ifndef _COLLECTOR_PIPELINE_H
#define _COLLECTOR_PIPELINE_H

#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>

#include "StoppableThread.h"

namespace collector {

template <typename T>
class Queue {
 public:
  Queue() {}
  ~Queue() {}

  T front() {
    auto lock = read_lock();
    return inner_.front();
  }

  T back() {
    auto lock = read_lock();
    return inner_.back();
  }

  bool empty() {
    auto lock = read_lock();
    return inner_.empty();
  }

  size_t size() {
    auto lock = read_lock();
    return inner_.size();
  }

  void push(const T& elem) {
    auto lock = write_lock();
    auto e = elem;
    inner_.push(std::move(e));
    state_changed_.notify_one();
  }

  void push(T&& elem) {
    auto lock = write_lock();
    inner_.push(elem);
    state_changed_.notify_one();
  }

  template <class... Args>
  decltype(auto) emplace(Args&&... args) {
    auto lock = write_lock();
    decltype(auto) out = inner_.emplace(std::forward<Args>(args)...);
    state_changed_.notify_one();
    return out;
  }

  T pop() {
    auto lock = write_lock();
    if (empty()) {
      state_changed_.wait(lock, [this]() { return empty(); });
    }
    T data = inner_.front();
    inner_.pop();
    return data;
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

template <class T>
class Producer {
 public:
  Producer(std::shared_ptr<Queue<T>>& output) : output_(output) {}

  ~Producer() {
    if (thread_.running()) {
      Stop();
    }
  }

  virtual T next() = 0;

  void Start() {
    thread_.Start([this] { Run(); });
  }

  void Stop() {
    thread_.Stop();
  }

  void Run() {
    while (!thread_.should_stop()) {
      auto event = next();
      output_->push(event);
    }
  }

 protected:
  std::shared_ptr<Queue<T>>& output_;
  StoppableThread thread_;
};

template <class T>
class Consumer {
 public:
  Consumer(std::shared_ptr<Queue<T>>& input) : input_(input) {}

  ~Consumer() {
    if (thread_.running()) {
      Stop();
    }
  }

  virtual void handle(const T& event) = 0;

  void Start() {
    thread_.Start([this] { Run(); });
  }

  void Stop() {
    thread_.Stop();
  }

  void Run() {
    while (!thread_.should_stop()) {
      auto event = input_->pop();
      handle(event);
    }
  }

 protected:
  std::shared_ptr<Queue<T>>& input_;
  StoppableThread thread_;
};

template <class In, class Out>
class Transformer {
 public:
  Transformer(std::shared_ptr<Queue<In>>& input, std::shared_ptr<Queue<Out>>& output)
      : input_(input), output_(output) {}

  ~Transformer() { Stop(); }

  virtual std::optional<Out> transform(const In& event) = 0;

  void Start() {
    thread_.Start([this] { Run(); });
  }

  void Stop() {
    thread_.Stop();
  }

  void Run() {
    while (!thread_.should_stop()) {
      auto event = input_->pop();
      auto transformed = transform(event);
      if (transformed.has_value()) {
        output_.push(transformed.value());
      }
    }
  }

 protected:
  std::shared_ptr<Queue<In>> input_;
  std::shared_ptr<Queue<Out>> output_;

  StoppableThread thread_;
};

template <class T>
using Filter = Transformer<T, T>;

}  // namespace collector

#endif
