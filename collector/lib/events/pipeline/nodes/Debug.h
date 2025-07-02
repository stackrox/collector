#pragma once

#include "events/pipeline/Nodes.h"

namespace collector::pipeline {

template <class In>
class DebugNode : public Transformer<In, In> {
 public:
  DebugNode(const std::shared_ptr<Queue<In>>& input, const std::shared_ptr<Queue<In>>& output)
      : Transformer<In, In>(input, output) {}

  std::optional<In> transform(const In& input) {
    CLOG(INFO) << input;
    return {input};
  }
};
}  // namespace collector::pipeline
