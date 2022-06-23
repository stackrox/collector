/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#include <chrono>
#include <mutex>
#include <string>

#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"

#include "CollectorConfig.h"
#include "DuplexGRPC.h"
#include "NetworkStatusNotifier.h"
#include "Utility.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace collector;
using grpc_duplex_impl::Result;
using grpc_duplex_impl::Status;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnPointee;

// until we use C++20
class Semaphore {
 public:
  Semaphore(int value = 1) : value_(value) {}

  void release() {
    std::unique_lock<std::mutex> lock(mutex_);
    value_++;
    cond_.notify_one();
  }

  void acquire() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (value_ <= 0)
      cond_.wait(lock);
    value_--;
  }

  template <class Rep, class Period>
  bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time) {
    auto deadline = std::chrono::steady_clock::now() + rel_time;
    std::unique_lock<std::mutex> lock(mutex_);

    while (value_ <= 0)
      if (cond_.wait_until(lock, deadline) == std::cv_status::timeout)
        return false;
    value_--;
    return true;
  }

 private:
  int value_;
  std::condition_variable cond_;
  std::mutex mutex_;
};

class MockConnScraper : public IConnScraper {
 public:
  MOCK_METHOD(bool, Scrape, (std::vector<Connection> * connections, std::vector<ContainerEndpoint>* listen_endpoints), (override));
};

class MockDuplexClientWriter : public IDuplexClientWriter<sensor::NetworkConnectionInfoMessage> {
 public:
  MOCK_METHOD(grpc_duplex_impl::Result, Write, (const sensor::NetworkConnectionInfoMessage& obj, const gpr_timespec& deadline), (override));
  MOCK_METHOD(grpc_duplex_impl::Result, WriteAsync, (const sensor::NetworkConnectionInfoMessage& obj), (override));
  MOCK_METHOD(grpc_duplex_impl::Result, WaitUntilStarted, (const gpr_timespec& deadline), (override));
  MOCK_METHOD(bool, Sleep, (const gpr_timespec& deadline), (override));
  MOCK_METHOD(grpc_duplex_impl::Result, WritesDoneAsync, (), (override));
  MOCK_METHOD(grpc_duplex_impl::Result, WritesDone, (const gpr_timespec& deadline), (override));
  MOCK_METHOD(grpc_duplex_impl::Result, FinishAsync, (), (override));
  MOCK_METHOD(grpc_duplex_impl::Result, WaitUntilFinished, (const gpr_timespec& deadline), (override));
  MOCK_METHOD(grpc_duplex_impl::Result, Finish, (grpc::Status * status, const gpr_timespec& deadline), (override));
  MOCK_METHOD(grpc::Status, Finish, (const gpr_timespec& deadline), (override));
  MOCK_METHOD(void, TryCancel, (), (override));
  MOCK_METHOD(grpc_duplex_impl::Result, Shutdown, (), (override));
};

class MockNetworkConnectionInfoServiceComm : public INetworkConnectionInfoServiceComm {
 public:
  MOCK_METHOD(void, ResetClientContext, (), (override));
  MOCK_METHOD(bool, WaitForConnectionReady, (const std::function<bool()>& check_interrupted), (override));
  MOCK_METHOD(void, TryCancel, (), (override));
  MOCK_METHOD(sensor::NetworkConnectionInfoService::StubInterface*, GetStub, (), (override));
  MOCK_METHOD(std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>>, PushNetworkConnectionInfoOpenStream, (std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func), (override));
};

TEST(NetworkStatusNotifier, SimpleStartStop) {
  bool running = true;
  CollectorConfig config_(0);
  std::shared_ptr<MockConnScraper> conn_scraper = std::make_shared<MockConnScraper>();
  auto conn_tracker = std::make_shared<ConnectionTracker>();
  auto comm = std::make_shared<MockNetworkConnectionInfoServiceComm>();
  Semaphore sem(0);  // to wait for the service to accomplish its job.

  EXPECT_CALL(*comm, WaitForConnectionReady).WillRepeatedly(Return(true));
  // gRPC shuts down the loop, so we will want writer->Sleep to return with false
  EXPECT_CALL(*comm, TryCancel).Times(1).WillOnce([&running] { running = false; });

  // Create and return a writer object
  EXPECT_CALL(*comm, PushNetworkConnectionInfoOpenStream).Times(1).WillOnce([&sem, &running](std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) -> std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> {
    auto duplex_writer = MakeUnique<MockDuplexClientWriter>();

    // the service is sending Sensor a message
    EXPECT_CALL(*duplex_writer, Write).Times(1).WillOnce([&sem, &running](const sensor::NetworkConnectionInfoMessage& msg, const gpr_timespec& deadline) -> Result {
      for (auto cnx : msg.info().updated_connections()) {
        std::cout << cnx.container_id() << std::endl;
      }
      sem.release();  // notify that the test should end
      return Result(true);
    });
    EXPECT_CALL(*duplex_writer, Sleep).WillRepeatedly(ReturnPointee(&running));
    EXPECT_CALL(*duplex_writer, WaitUntilStarted).WillRepeatedly(Return(Result(true)));

    return duplex_writer;
  });

  // Connections/Endpoints returned by the scrapper
  EXPECT_CALL(*conn_scraper, Scrape).WillRepeatedly([&sem](std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints) -> bool {
    connections->emplace_back("containerId", Endpoint(Address(10, 0, 1, 32), 1024), Endpoint(Address(139, 45, 27, 4), 999), L4Proto::TCP, true);
    return true;
  });

  auto net_status_notifier = MakeUnique<NetworkStatusNotifier>(conn_scraper,
                                                               config_.ScrapeInterval(), config_.ScrapeListenEndpoints(),
                                                               config_.TurnOffScrape(),
                                                               conn_tracker,
                                                               config_.AfterglowPeriod(), config_.EnableAfterglow(),
                                                               comm);

  net_status_notifier->Start();

  // we could send probe events using conn_tracker->UpdateConnection

  EXPECT_TRUE(sem.try_acquire_for(std::chrono::seconds(5)));

  running = false;
  net_status_notifier->Stop();

  SUCCEED();
}
