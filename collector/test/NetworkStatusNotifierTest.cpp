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

#include <google/protobuf/util/time_util.h>

#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"

#include "CollectorConfig.h"
#include "DuplexGRPC.h"
#include "NetworkStatusNotifier.h"
#include "Utility.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

namespace {

using grpc_duplex_impl::Result;
using grpc_duplex_impl::Status;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::UnorderedElementsAre;

/* Semaphore: A subset of the semaphore feature.
   We use it to wait (with a timeout) for the NetworkStatusNotifier service thread to achieve the test scenario.
   (C++20 comes with a compatible class (std::counting_semaphore) and we probably want to use this once we
   update the C++ version). */
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

class MockCollectorConfig : public collector::CollectorConfig {
 public:
  MockCollectorConfig() : collector::CollectorConfig(0) {}

  void DisableAfterglow() {
    enable_afterglow_ = false;
  }
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

/* gRPC payload objects are not strictly the ones of our internal model.
   This helper class converts back NetworkConnectionInfoMessage into internal model in order
   to compare them with the expected content. */
class NetworkConnectionInfoMessageParser {
 public:
  NetworkConnectionInfoMessageParser(const sensor::NetworkConnectionInfoMessage& msg) {
    for (auto conn : msg.info().updated_connections()) {
      updated_connections_.emplace(
          Connection(
              conn.container_id(),
              Endpoint(IPNetFromProto(conn.local_address()), conn.local_address().port()),
              Endpoint(IPNetFromProto(conn.remote_address()), conn.remote_address().port()),
              L4ProtocolFromProto(conn.protocol()),
              conn.role() == sensor::ROLE_SERVER),
          !conn.has_close_timestamp());
    }
  }

  /* our internal model for connection life-cycle events uses the Connection object as the key for a hashmap.
     The value in the hashmap describes the kind of event: true stands for a creation, false for a deletion. */
  const std::unordered_map<Connection, bool, Hasher>& get_updated_connections() const { return updated_connections_; }

 private:
  std::unordered_map<Connection, bool, Hasher> updated_connections_;

  static IPNet IPNetFromProto(const sensor::NetworkAddress& na) {
    Address addr;
    const bool is_net = (na.ip_network().length() != 0);

    const std::string& bytes_stream = is_net ? na.ip_network() : na.address_data();

    switch (bytes_stream.length()) {
      case 0:
        return IPNet();
      case 4:
      case 5: {
        uint32_t ip;
        memcpy((void*)&ip, bytes_stream.c_str(), 4);
        addr = Address(ip);
        break;
      }
      case 16:
      case 17: {
        uint32_t ip[4];
        memcpy((void*)ip, bytes_stream.c_str(), 16);
        addr = Address(ip);
        break;
      }
      default:
        throw std::runtime_error("Invalid address length");
    }

    if (is_net) {
      const char* prefix_len = bytes_stream.c_str() + (bytes_stream.length() - 1);

      return IPNet(addr, *prefix_len);
    } else {
      return IPNet(addr);
    }
  }

  static L4Proto L4ProtocolFromProto(storage::L4Protocol proto) {
    switch (proto) {
      case storage::L4_PROTOCOL_TCP:
        return L4Proto::TCP;
      case storage::L4_PROTOCOL_UDP:
        return L4Proto::UDP;
      case storage::L4_PROTOCOL_ICMP:
        return L4Proto::ICMP;
      default:
        return L4Proto::UNKNOWN;
    }
  }
};

/* Simple validation that the service starts and sends at least one event */
TEST(NetworkStatusNotifier, SimpleStartStop) {
  bool running = true;
  CollectorConfig config_(0);
  std::shared_ptr<MockConnScraper> conn_scraper = std::make_shared<MockConnScraper>();
  auto conn_tracker = std::make_shared<ConnectionTracker>();
  auto comm = std::make_shared<MockNetworkConnectionInfoServiceComm>();
  Semaphore sem(0);  // to wait for the service to accomplish its job.

  // the connection is always ready
  EXPECT_CALL(*comm, WaitForConnectionReady).WillRepeatedly(Return(true));
  // gRPC shuts down the loop, so we will want writer->Sleep to return with false
  EXPECT_CALL(*comm, TryCancel).Times(1).WillOnce([&running] { running = false; });

  /* This is what NetworkStatusNotifier calls to create streams
     receive_func is a callback that we can use to simulate messages coming from the sensor
     We return an object that will get called when connections and endpoints are reported */
  EXPECT_CALL(*comm, PushNetworkConnectionInfoOpenStream)
      .Times(1)
      .WillOnce([&sem, &running](std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) -> std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> {
        auto duplex_writer = MakeUnique<MockDuplexClientWriter>();

        // the service is sending Sensor a message
        EXPECT_CALL(*duplex_writer, Write).WillRepeatedly([&sem, &running](const sensor::NetworkConnectionInfoMessage& msg, const gpr_timespec& deadline) -> Result {
          for (auto cnx : msg.info().updated_connections()) {
            std::cout << cnx.container_id() << std::endl;
          }
          sem.release();  // notify that the test should end
          return Result(Status::OK);
        });
        EXPECT_CALL(*duplex_writer, Sleep).WillRepeatedly(ReturnPointee(&running));
        EXPECT_CALL(*duplex_writer, WaitUntilStarted).WillRepeatedly(Return(Result(Status::OK)));

        return duplex_writer;
      });

  // Connections/Endpoints returned by the scrapper
  EXPECT_CALL(*conn_scraper, Scrape).WillRepeatedly([&sem](std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints) -> bool {
    // this is the data which will trigger NetworkStatusNotifier to create a connection event (purpose of the test)
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

  EXPECT_TRUE(sem.try_acquire_for(std::chrono::seconds(5)));

  net_status_notifier->Stop();
}

/* This test checks whether deltas are computed appropriately in case the "known network" list is received after a connection
   is already reported (and matches one of the networks).
   - scrapper initialy reports a connection
   - the connection is reported
   - we feed a "known network" into the NetworkStatusNotifier
   - we check that the former declared connection is deleted and redeclared as part of this network */
TEST(NetworkStatusNotifier, UpdateIPnoAfterglow) {
  bool running = true;
  MockCollectorConfig config;
  std::shared_ptr<MockConnScraper> conn_scraper = std::make_shared<MockConnScraper>();
  auto conn_tracker = std::make_shared<ConnectionTracker>();
  auto comm = std::make_shared<MockNetworkConnectionInfoServiceComm>();
  std::function<void(const sensor::NetworkFlowsControlMessage*)> network_flows_callback;
  Semaphore sem(0);  // to wait for the service to accomplish its job.

  // the connection as scrapped (public)
  Connection conn1("containerId", Endpoint(Address(10, 0, 1, 32), 1024), Endpoint(Address(139, 45, 27, 4), 999), L4Proto::TCP, true);
  // the same server connection normalized
  Connection conn2("containerId", Endpoint(Address(), 1024), Endpoint(Address(255, 255, 255, 255), 0), L4Proto::TCP, true);
  // the same server connection normalized and grouped in a known subnet
  Connection conn3("containerId", Endpoint(Address(), 1024), Endpoint(IPNet(Address(139, 45, 0, 0), 16), 0), L4Proto::TCP, true);

  config.DisableAfterglow();

  // the connection is always ready
  EXPECT_CALL(*comm, WaitForConnectionReady).WillRepeatedly(Return(true));
  // gRPC shuts down the loop, so we will want writer->Sleep to return with false
  EXPECT_CALL(*comm, TryCancel).Times(1).WillOnce([&running] { running = false; });

  /* This is what NetworkStatusNotifier calls to create streams
   receive_func is a callback that we can use to simulate messages coming from the sensor
   We return an object that will get called when connections and endpoints are reported */
  EXPECT_CALL(*comm, PushNetworkConnectionInfoOpenStream)
      .Times(1)
      .WillOnce([&sem,
                 &running,
                 &conn2,
                 &conn3,
                 &network_flows_callback](std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) -> std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> {
        auto duplex_writer = MakeUnique<MockDuplexClientWriter>();
        network_flows_callback = receive_func;

        // the service is sending Sensor a message
        EXPECT_CALL(*duplex_writer, Write)
            .WillOnce([&conn2, &sem](const sensor::NetworkConnectionInfoMessage& msg, const gpr_timespec& deadline) -> Result {
              // the connection reported by the scrapper is annouced as generic public
              EXPECT_THAT(NetworkConnectionInfoMessageParser(msg).get_updated_connections(), UnorderedElementsAre(std::make_pair(conn2, true)));
              return Result(Status::OK);
            })
            .WillOnce([&conn2, &conn3, &sem](const sensor::NetworkConnectionInfoMessage& msg, const gpr_timespec& deadline) -> Result {
              // after the network is declared, the connection switches to the new state
              // conn3 appears and conn2 is destroyed
              EXPECT_THAT(NetworkConnectionInfoMessageParser(msg).get_updated_connections(), UnorderedElementsAre(std::make_pair(conn3, true), std::make_pair(conn2, false)));

              // Done
              sem.release();

              return Result(Status::OK);
            })
            .WillRepeatedly(Return(Result(Status::OK)));

        EXPECT_CALL(*duplex_writer, Sleep)
            .WillOnce(ReturnPointee(&running))  // first time, we let the scrapper do its job
            .WillOnce([&running, &network_flows_callback](const gpr_timespec& deadline) {
              // The connection is known now, let's declare a "known network"
              sensor::NetworkFlowsControlMessage msg;
              unsigned char content[] = {139, 45, 0, 0, 16};  // address in network order, plus prefix length
              std::string network((char*)content, sizeof(content));

              auto ip_networks = msg.mutable_ip_networks();
              ip_networks->set_ipv4_networks(network);

              network_flows_callback(&msg);
              return running;
            })
            .WillRepeatedly(ReturnPointee(&running));

        EXPECT_CALL(*duplex_writer, WaitUntilStarted).WillRepeatedly(Return(Result(Status::OK)));

        return duplex_writer;
      });

  // Connections/Endpoints returned by the scrapper (first detection of the connection)
  EXPECT_CALL(*conn_scraper, Scrape).WillRepeatedly([&conn1](std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints) -> bool {
    connections->emplace_back(conn1);
    return true;
  });

  auto net_status_notifier = MakeUnique<NetworkStatusNotifier>(conn_scraper,
                                                               config.ScrapeInterval(), config.ScrapeListenEndpoints(),
                                                               config.TurnOffScrape(),
                                                               conn_tracker,
                                                               config.AfterglowPeriod(), config.EnableAfterglow(),
                                                               comm);

  net_status_notifier->Start();

  // Wait for the first scrape to occur
  EXPECT_TRUE(sem.try_acquire_for(std::chrono::seconds(5)));

  net_status_notifier->Stop();
}

}  // namespace

}  // namespace collector