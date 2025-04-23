#ifndef COLLECTOR_CHANNEL_H
#define COLLECTOR_CHANNEL_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <iterator>
#include <mutex>
#include <queue>

namespace collector {
template <typename T>
class Channel {
 public:
  using value_type = T;

  Channel(std::size_t capacity = 0)
      : capacity_(capacity) {
  }

  bool IsClosed() {
    return closed_.load();
  }

  void Close() {
    closed_.store(true);
    cv_.notify_all();
  }

  friend Channel<T>& operator<<(Channel<T>& ch, const T& data) {
    if (ch.IsClosed()) {
      // If the channel is closed we simply drop messages
      return ch;
    }

    std::unique_lock<std::mutex> lock{ch.mutex_};
    if (ch.capacity_ > 0 && ch.queue_.size() >= ch.capacity_) {
      ch.cv_.wait(lock, [&ch] { return ch.queue_.size() < ch.capacity_; });
    }

    ch.queue_.push(data);
    ch.cv_.notify_one();

    return ch;
  }

  friend Channel<T>& operator<<(Channel<T>& ch, T&& data) {
    if (ch.IsClosed()) {
      // If the channel is closed we simply drop messages
      return ch;
    }

    std::unique_lock<std::mutex> lock{ch.mutex_};
    if (ch.capacity_ > 0 && ch.queue_.size() >= ch.capacity_) {
      ch.cv_.wait(lock, [&ch] { return ch.queue_.size() < ch.capacity_; });
    }

    ch.queue_.push(std::move(data));
    ch.cv_.notify_one();

    return ch;
  }

  friend Channel<T>& operator>>(Channel<T>& ch, T& out) {
    std::unique_lock<std::mutex> lock{ch.mutex_};
    if (ch.IsClosed() && ch.queue_.empty()) {
      return ch;
    }

    ch.cv_.wait(lock, [&ch] { return !ch.queue_.empty() || ch.IsClosed(); });
    if (!ch.queue_.empty()) {
      out = std::move(ch.queue_.front());
      ch.queue_.pop();
    }

    ch.cv_.notify_one();
    return ch;
  }

  bool Empty() {
    std::unique_lock<std::mutex> lock{mutex_};
    return queue_.empty();
  }

  struct Iterator {
    Iterator(Channel<T>& ch) : ch_(ch) {}

    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using reference = T&;
    using pointer = T*;

    Iterator operator++() { return *this; }
    reference operator*() {
      ch_ >> value_;
      return value_;
    }

    bool operator!=(Iterator& /*unused*/) const {
      std::unique_lock<std::mutex> lock{ch_.mutex_};
      ch_.cv_.wait(lock, [this] { return !ch_.queue_.empty() || ch_.IsClosed(); });

      return !(ch_.IsClosed() && ch_.queue_.empty());
    }

   private:
    Channel<T>& ch_;
    value_type value_;
  };

  Iterator begin() { return Iterator{*this}; }
  Iterator end() { return Iterator{*this}; }

 private:
  std::size_t capacity_;
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> closed_{false};
};
}  // namespace collector

#endif
