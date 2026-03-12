#pragma once

#include <memory>
#include <vector>

#include "Queue.h"
#include "StoppableThread.h"

namespace collector::pipeline {

template <class Out>
class Producer {
 public:
  using InputType = void;
  using OutputType = Out;

  Producer(const std::shared_ptr<void> _ptr /* ignored */, const std::shared_ptr<Queue<Out>>& output) : output_(output) {}

  ~Producer() {
    if (thread_.running()) {
      Stop();
    }
  }

  virtual std::optional<Out> next() = 0;

  void Start() {
    thread_.Start([this] { Run(); });
  }

  void Stop() {
    thread_.Stop();
  }

  void Run() {
    while (!thread_.should_stop()) {
      auto event = next();
      if (!event.has_value()) {
        break;
      }
      output_->push(event.value());
    }
  }

 protected:
  std::shared_ptr<Queue<Out>>& output_;
  StoppableThread thread_;
};

template <class In>
class Consumer {
 public:
  using InputType = In;
  using OutputType = void;

  Consumer(const std::shared_ptr<Queue<In>>& input, const std::shared_ptr<void> _ptr /*ignored*/) : input_(input) {}

  ~Consumer() {
    if (thread_.running()) {
      Stop();
    }
  }

  virtual void consume(const In& event) = 0;

  void Start() {
    thread_.Start([this] { Run(); });
  }

  void Stop() {
    thread_.Stop();
  }

  void Run() {
    while (!thread_.should_stop()) {
      if (!input_) {
        CLOG(DEBUG) << "No input queue for Consumer";
        break;
      }
      auto event = input_->pop();
      if (!event.has_value()) {
        continue;
      }
      consume(event.value());
    }
  }

 protected:
  const std::shared_ptr<Queue<In>>& input_;
  StoppableThread thread_;
};

template <class In, class Out>
class Transformer {
 public:
  using InputType = In;
  using OutputType = Out;

  Transformer(const std::shared_ptr<Queue<In>>& input, const std::shared_ptr<Queue<Out>>& output)
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
      if (!event.has_value()) {
        continue;
      }

      auto transformed = transform(event.value());
      if (transformed.has_value()) {
        if (output_) {
          output_->push(transformed.value());
        }
      }
    }
  }

 protected:
  const std::shared_ptr<Queue<In>> input_;
  const std::shared_ptr<Queue<Out>> output_;

  StoppableThread thread_;
};

template <class InOut>
using Filter = Transformer<InOut, InOut>;

template <class InOut>
class Splitter : public Transformer<InOut, InOut> {
 public:
  using InputType = InOut;
  using OutputType = InOut;

  Splitter(const std::shared_ptr<Queue<InOut>> input,
           std::vector<const std::shared_ptr<Queue<InOut>>> outputs) : Transformer<InOut, InOut>(input, nullptr), outputs_(outputs) {
  }

  std::optional<InOut> transform(const InOut& event) override {
    for (const auto& queue : outputs_) {
      queue->push(event);
    }
    return std::nullopt;
  }

 private:
  std::vector<const std::shared_ptr<Queue<InOut>>> outputs_;
};

}  // namespace collector::pipeline
