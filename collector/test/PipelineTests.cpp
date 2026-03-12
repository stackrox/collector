#include <chrono>
#include <memory>
#include <optional>
#include <ratio>
#include <thread>
#include <vector>

#include "Pipeline.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// namespace collector {
//
// class IntProducer : public Producer<int> {
//  public:
//   IntProducer(std::shared_ptr<Queue<int>>& input, int limit) : Producer(input), limit_(limit) {}
//
//   std::optional<int> next() override {
//     n_++;
//     if (n_ > limit_) {
//       return std::nullopt;
//     }
//     return {n_};
//   }
//
//  private:
//   int n_ = 0;
//   int limit_;
// };
//
// class IntConsumer : public Consumer<int> {
//  public:
//   IntConsumer(std::shared_ptr<Queue<int>>& input, std::vector<int>& output) : Consumer(input), events_(output) {}
//
//   void handle(const int& event) override {
//     events_.push_back(event);
//   }
//
//  private:
//   std::vector<int>& events_;
// };
//
// class EvenIntFilter : public Filter<int> {
//  public:
//   std::optional<int> transform(const int& event) {
//     if (event % 2 == 0) {
//       return {event};
//     }
//     return std::nullopt;
//   }
// };
//
// TEST(PipelineTests, TestBasic) {
//   std::shared_ptr<Queue<int>> queue = std::make_shared<Queue<int>>();
//
//   std::vector<int> output;
//
//   std::unique_ptr<Producer<int>> producer = std::make_unique<IntProducer>(queue, 10);
//   std::unique_ptr<Consumer<int>> consumer = std::make_unique<IntConsumer>(queue, output);
//
//   producer->Start();
//   consumer->Start();
//
//   std::this_thread::sleep_for(std::chrono::milliseconds(200));
//
//   consumer->Stop();
//   producer->Stop();
//
//   EXPECT_TRUE(output.size() == 10);
// }
//
// }  // namespace collector
