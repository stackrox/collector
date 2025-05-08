#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "output/IClient.h"
#include "output/Output.h"

namespace collector::output {
class MockSensorClient : public IClient {
 public:
  MOCK_METHOD(bool, Recreate, ());
  MOCK_METHOD(SignalHandler::Result, SendMsg, (const MsgToSensor&));
  MOCK_METHOD(bool, IsReady, ());
};

class CollectorOutputTest : public testing::Test {
 public:
  CollectorOutputTest()
      : sensor_client(std::make_unique<MockSensorClient>()) {}

 protected:
  std::unique_ptr<MockSensorClient> sensor_client;
};

TEST_F(CollectorOutputTest, SensorClient) {
  sensor::ProcessSignal msg;

  EXPECT_CALL(*sensor_client, SendMsg).Times(1).WillOnce(testing::Return(SignalHandler::PROCESSED));

  Output output{std::move(sensor_client)};
  auto result = output.SendMsg(msg);

  EXPECT_EQ(result, SignalHandler::PROCESSED);
}
}  // namespace collector::output
