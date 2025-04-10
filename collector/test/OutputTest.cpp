#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "SignalServiceClient.h"
#include "output/IClient.h"
#include "output/Output.h"

namespace collector::output {
class MockSensorClient : public IClient {
 public:
  MOCK_METHOD(bool, Recreate, ());
  MOCK_METHOD(SignalHandler::Result, SendMsg, (const sensor::ProcessSignal&));
};

class MockSignalClient : public ISignalServiceClient {
 public:
  MOCK_METHOD(bool, Recreate, ());
  MOCK_METHOD(SignalHandler::Result, PushSignals, (const sensor::SignalStreamMessage&));
};

class CollectorOutputTest : public testing::Test {
 public:
  CollectorOutputTest()
      : sensor_client(std::make_unique<MockSensorClient>()),
        signal_client(std::make_unique<MockSignalClient>()) {}

 protected:
  std::unique_ptr<MockSensorClient> sensor_client;
  std::unique_ptr<MockSignalClient> signal_client;
};

TEST_F(CollectorOutputTest, SensorClient) {
  sensor::ProcessSignal msg;

  EXPECT_CALL(*sensor_client, SendMsg).Times(1).WillOnce(testing::Return(SignalHandler::PROCESSED));

  Output output{std::move(sensor_client), std::move(signal_client)};
  auto result = output.SendMsg(msg);

  EXPECT_EQ(result, SignalHandler::PROCESSED);
}

TEST_F(CollectorOutputTest, SignalClient) {
  sensor::SignalStreamMessage msg;

  EXPECT_CALL(*signal_client, PushSignals).Times(1).WillOnce(testing::Return(SignalHandler::PROCESSED));

  Output output{std::move(sensor_client), std::move(signal_client)};

  auto result = output.SendMsg(msg);

  EXPECT_EQ(result, SignalHandler::PROCESSED);
}
}  // namespace collector::output
