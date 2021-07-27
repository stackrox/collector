#include <memory>
#include <utility>

#include "MemoryStats.h"
#include "NodeMetrics.h"
#include "ProcessSignalFormatter.h"
#include "Utility.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

namespace {

TEST(TestStats, TestNodeMetrics) {
  // There must be at least one processor.
  EXPECT_GE(NodeMetrics::NumProcessors(), 1);
}

TEST(TestStats, TestThrottleMetrics) {
  CPUThrottleMetrics::cpu_throttle_metric_t m;
  EXPECT_TRUE(CPUThrottleMetrics::ReadStatFile(&m));
}

TEST(TestStats, TestMemoryStats) {
#ifdef COLLECTOR_PROFILING
  EXPECT_GT(MemoryStats::HeapSize(), MemoryStats::AllocatedSize());
  EXPECT_GT(MemoryStats::PhysicalSize(), MemoryStats::HeapSize());
  auto allocated_size_before = MemoryStats::AllocatedSize();
  EXPECT_GT(allocated_size_before, 0);
  constexpr int array_size = 4096;
  auto mem0 = std::unique_ptr<char[]>(new char[array_size]);
  EXPECT_TRUE(mem0 != nullptr);
  EXPECT_EQ(array_size, MemoryStats::AllocatedSize() - allocated_size_before);
  EXPECT_GT(MemoryStats::PhysicalSize(), 0);
#endif
}

}  // namespace

}  // namespace collector
