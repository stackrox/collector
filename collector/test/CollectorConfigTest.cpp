#include "CollectorArgs.h"
#include "CollectorConfig.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

namespace collector {

class MockCollectorConfig : public CollectorConfig {
 public:
  MockCollectorConfig() = default;

  void MockSetSinspBufferSize(unsigned int value) {
    SetSinspBufferSize(value);
  }

  void MockSetSinspTotalBufferSize(unsigned int value) {
    SetSinspTotalBufferSize(value);
  }

  void MockSetHostConfig(HostConfig* config) {
    SetHostConfig(config);
  }

  void MockSetSinspCpuPerBuffer(unsigned int value) {
    SetSinspCpuPerBuffer(value);
  }
};

TEST(CollectorConfigTest, TestSinspCpuPerBufferRaw) {
  using namespace collector;

  MockCollectorConfig config;
  HostConfig hconfig;
  hconfig.SetNumPossibleCPUs(0);
  config.MockSetSinspBufferSize(0);
  config.MockSetSinspCpuPerBuffer(1);
  config.MockSetHostConfig(&hconfig);

  // Buffer size is not initialized
  EXPECT_EQ(1, config.GetSinspCpuPerBuffer());

  // Number of CPUs is not initialized
  config.MockSetSinspBufferSize(1024);
  EXPECT_EQ(1, config.GetSinspCpuPerBuffer());
}

TEST(CollectorConfigTest, TestSinspCpuPerBufferAdjusted) {
  using namespace collector;

  MockCollectorConfig config;
  HostConfig hconfig;
  config.MockSetSinspTotalBufferSize(512 * 1024 * 1024);
  config.MockSetSinspBufferSize(8 * 1024 * 1024);
  config.MockSetSinspCpuPerBuffer(1);

  // Low number of CPUs, raw value
  hconfig.SetNumPossibleCPUs(16);
  config.MockSetHostConfig(&hconfig);
  EXPECT_EQ(1, config.GetSinspCpuPerBuffer());

  // High number of CPUs, adjusted value
  hconfig.SetNumPossibleCPUs(150);
  config.MockSetHostConfig(&hconfig);
  EXPECT_EQ(3, config.GetSinspCpuPerBuffer());
}

}  // namespace collector
