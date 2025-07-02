#pragma once

#include <tuple>
#include <utility>

#include "events/pipeline/Queue.h"

namespace collector::pipeline {

template <typename... Nodes>
class Pipeline {
  using QueueTuple = std::tuple<const std::shared_ptr<Queue<typename Nodes::InputType>>...>;
  struct Graph {
    std::tuple<std::unique_ptr<Nodes>...> nodes;
    QueueTuple queues;
  };

  static Graph build_graph() {
    auto queues = std::make_tuple(std::make_shared<Queue<typename Nodes::InputType>>()...);
    auto nodes = build_nodes(queues, std::index_sequence_for<Nodes...>{});
    return {
        std::move(nodes),
        std::move(queues),
    };
  }

  template <std::size_t... Seq>
  static auto build_nodes(const std::tuple<const std::shared_ptr<Queue<typename Nodes::InputType>>...>& queues,
                          std::index_sequence<Seq...>) {
    // This is a fold expression over the comma operator.
    // It expands to: std::make_tuple(Node_0(...), Node_1(...), Node_2(...), ...)
    return std::make_tuple(
        std::make_unique<Nodes>(
            std::get<Seq>(queues),
            get_output_queue<Seq>(queues))...);
  }

  // Helper to safely get the output queue for a given node index.
  template <std::size_t I>
  static auto get_output_queue(
      const std::tuple<std::shared_ptr<Queue<typename Nodes::InputType>>...>& queues) {
    if constexpr (I + 1 < sizeof...(Nodes)) {
      return std::get<I + 1>(queues);
    } else {
      // This is the last node in the pipeline (a Consumer).
      // Its output type is `void`, so we pass a nullptr.
      return nullptr;
    }
  }

 public:
  using In = typename std::tuple_element_t<0, std::tuple<Nodes...>>::InputType;

  Pipeline() : graph_(build_graph()) {
  }

  void Start() {
    start_nodes<0>();
  }

  void Stop() {
    stop_nodes<0>();
  }

  void Push(const In& input) const {
    std::get<0>(graph_.queues)->push(input);
  }

 private:
  Graph graph_;

  template <std::size_t I = 0>
  void start_nodes() {
    if constexpr (I < sizeof...(Nodes)) {
      std::get<I>(graph_.nodes)->Start();
      start_nodes<I + 1>();
    }
  }

  template <std::size_t I = 0>
  void stop_nodes() {
    if constexpr (I < sizeof...(Nodes)) {
      std::get<I>(graph_.nodes)->Stop();
      start_nodes<I + 1>();
    }
  }
};
}  // namespace collector::pipeline
