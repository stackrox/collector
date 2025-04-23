#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>

#include <google/protobuf/util/time_util.h>

#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"
#include "internalapi/sensor/network_connection_iservice.pb.h"
#include "internalapi/sensor/network_enums.pb.h"

#include "CollectorConfig.h"
#include "DuplexGRPC.h"
#include "NetworkConnection.h"
#include "NetworkStatusNotifier.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "output/IClient.h"
#include "output/grpc/IGrpcClient.h"
#include "system-inspector/Service.h"

namespace collector {

namespace {

using grpc_duplex_impl::Result;
using grpc_duplex_impl::Status;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::UnorderedElementsAre;

class MockCollectorConfig : public collector::CollectorConfig {
 public:
  MockCollectorConfig() = default;

  void DisableAfterglow() {
    enable_afterglow_ = false;
  }

  void SetMaxConnectionsPerMinute(int64_t limit) {
    max_connections_per_minute_ = limit;
  }

  void SetScrapInterval(int interval) {
    scrape_interval_ = interval;
  }
};

class MockConnScraper : public IConnScraper {
 public:
  MOCK_METHOD(bool, Scrape, (std::vector<Connection> * connections, std::vector<ContainerEndpoint>* listen_endpoints), (override));
};

class MockOutput : public output::Output {
 public:
  MockOutput(const MockOutput&) = delete;
  MockOutput(MockOutput&&) = delete;
  MockOutput& operator=(const MockOutput&) = delete;
  MockOutput& operator=(MockOutput&&) = delete;
  virtual ~MockOutput() = default;

  MockOutput(Channel<sensor::NetworkFlowsControlMessage>& ch)
      : Output(ch) {}

  MOCK_METHOD(SignalHandler::Result, SendMsg, (const output::MsgToSensor& msg));
  MOCK_METHOD(bool, IsReady, ());
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
        return {};
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

      return {addr, static_cast<size_t>(*prefix_len)};
    }
    return IPNet(addr);
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

MATCHER_P(MsgToSensorMatcher, expected, "Match elements in a MsgToSensor to a Connection") {
  if (const auto* msg = std::get_if<sensor::NetworkConnectionInfoMessage>(&arg)) {
    const auto connections = NetworkConnectionInfoMessageParser(*msg).get_updated_connections();
    EXPECT_THAT(connections, expected);
    return true;
  }
  return false;
}

}  // namespace

class NetworkStatusNotifierTest : public testing::Test {
 public:
  NetworkStatusNotifierTest()
      : conn_tracker(std::make_shared<ConnectionTracker>()),
        conn_scraper(std::make_unique<MockConnScraper>()),
        output(ch),
        inspector(config, &output),
        net_status_notifier(conn_tracker, config, &inspector, &output, nullptr) {
  }

 protected:
  MockCollectorConfig config;
  std::shared_ptr<ConnectionTracker> conn_tracker;
  std::unique_ptr<MockConnScraper> conn_scraper;
  Channel<sensor::NetworkFlowsControlMessage> ch;
  MockOutput output;
  system_inspector::Service inspector;
  NetworkStatusNotifier net_status_notifier;

  // Used for waiting on test done.
  std::mutex mutex;
  std::condition_variable cv;
};

/* Simple validation that the service starts and sends at least one event */
TEST_F(NetworkStatusNotifierTest, SimpleStartStop) {
  Connection conn{
      "containerId",
      Endpoint{Address{10, 0, 1, 32}, 1024},
      Endpoint{Address{139, 45, 27, 4}, 999},
      L4Proto::TCP,
      true};
  // Expect the connection to be normalized
  Connection expected{
      "containerId",
      Endpoint{Address{}, 1024},
      Endpoint{Address{255, 255, 255, 255}, 0},
      L4Proto::TCP,
      true};

  // Connections/Endpoints returned by the scrapper
  EXPECT_CALL(*conn_scraper, Scrape).WillRepeatedly([&conn](std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints) -> bool {
    // this is the data which will trigger NetworkStatusNotifier to create a connection event (purpose of the test)
    connections->push_back(conn);
    return true;
  });

  EXPECT_CALL(output, SendMsg(MsgToSensorMatcher(UnorderedElementsAre(std::make_pair(expected, true)))))
      .WillOnce([this](const output::MsgToSensor& a) {
        cv.notify_one();
        return SignalHandler::PROCESSED;
      });

  net_status_notifier.ReplaceConnScraper(std::move(conn_scraper));

  net_status_notifier.Start();

  std::unique_lock<std::mutex> lock{mutex};
  EXPECT_EQ(cv.wait_for(lock, std::chrono::seconds(5)), std::cv_status::no_timeout);

  net_status_notifier.Stop();
}

/* This test checks whether deltas are computed appropriately in case the "known network" list is received after a connection
   is already reported (and matches one of the networks).
   - scrapper initially reports a connection
   - the connection is reported
   - we feed a "known network" into the NetworkStatusNotifier
   - we check that the former declared connection is deleted and redeclared as part of this network */
TEST_F(NetworkStatusNotifierTest, UpdateIPnoAfterglow) {
  config.DisableAfterglow();
  config.SetScrapInterval(1);  // Immediately scrape
  std::function<void(const sensor::NetworkFlowsControlMessage*)>
      network_flows_callback;

  // the connection as scrapped (public)
  Connection connection_scrapped{
      "containerId",
      Endpoint{Address{10, 0, 1, 32}, 1024},
      Endpoint{Address{139, 45, 27, 4}, 999},
      L4Proto::TCP,
      true};
  // the same server connection normalized
  Connection connection_normalized{
      "containerId",
      Endpoint{Address{}, 1024},
      Endpoint{Address{255, 255, 255, 255}, 0},
      L4Proto::TCP,
      true};
  // the same server connection normalized and grouped in a known subnet
  Connection connection_grouped{
      "containerId",
      Endpoint{Address{}, 1024},
      Endpoint{IPNet{Address{139, 45, 0, 0}, 16}, 0},
      L4Proto::TCP,
      true};

  // Connections/Endpoints returned by the scrapper
  EXPECT_CALL(*conn_scraper, Scrape).WillRepeatedly([&connection_scrapped](std::vector<Connection>* connections, std::vector<ContainerEndpoint>* listen_endpoints) -> bool {
    // this is the data which will trigger NetworkStatusNotifier to create a connection event (purpose of the test)
    connections->push_back(connection_scrapped);
    return true;
  });

  EXPECT_CALL(output, SendMsg(MsgToSensorMatcher(testing::AnyOf(
                          UnorderedElementsAre(std::make_pair(connection_normalized, true)),
                          UnorderedElementsAre(
                              std::make_pair(connection_normalized, false),
                              std::make_pair(connection_grouped, true))))))
      .Times(2)
      .WillOnce([this](const output::MsgToSensor& a) {
        // Feed a message from Sensor to group the IP in
        sensor::NetworkFlowsControlMessage msg;
        std::array<unsigned char, 5> content{139, 45, 0, 0, 16};  // address in network order, plus prefix length
        std::string network(reinterpret_cast<char*>(content.data()), content.size());

        auto* ip_networks = msg.mutable_ip_networks();
        ip_networks->set_ipv4_networks(network);

        ch << msg;

        return SignalHandler::PROCESSED;
      })
      .WillOnce([this](const output::MsgToSensor& a) {
        // Done
        cv.notify_one();
        return SignalHandler::PROCESSED;
      });

  net_status_notifier.ReplaceConnScraper(std::move(conn_scraper));

  net_status_notifier.Start();

  std::unique_lock<std::mutex> lock{mutex};
  EXPECT_EQ(cv.wait_for(lock, std::chrono::seconds(5)), std::cv_status::no_timeout);

  net_status_notifier.Stop();
}

TEST_F(NetworkStatusNotifierTest, RateLimitedConnections) {
  // maximum of 2 connections per scrape interval
  // if we throw four connections from the same container into the conn
  // tracker delta, we expect only two to be returned
  // The scrape interval is 30 seconds so max_connections_per_minute_
  // should be 4 to make per_container_rate_limit 2
  config.SetMaxConnectionsPerMinute(4);

  Connection conn1("containerId", Endpoint(Address(10, 0, 1, 32), 1024), Endpoint(Address(192, 168, 0, 1), 1000), L4Proto::TCP, true);
  Connection conn2("containerId", Endpoint(Address(10, 0, 1, 32), 1024), Endpoint(Address(192, 168, 0, 2), 1001), L4Proto::TCP, true);
  Connection conn3("containerId", Endpoint(Address(10, 0, 1, 32), 1024), Endpoint(Address(192, 168, 0, 3), 1002), L4Proto::TCP, true);
  Connection conn4("containerId", Endpoint(Address(10, 0, 1, 32), 1024), Endpoint(Address(192, 168, 0, 4), 1003), L4Proto::TCP, true);

  Connection otherConn1("containerIdOther", Endpoint(Address(10, 10, 1, 32), 1024), Endpoint(Address(192, 168, 0, 1), 1000), L4Proto::TCP, true);
  Connection otherConn2("containerIdOther", Endpoint(Address(10, 10, 1, 32), 1024), Endpoint(Address(192, 168, 0, 2), 1001), L4Proto::TCP, true);

  ConnStatus statusActive = ConnStatus(1234, true);
  ConnStatus statusClosed = ConnStatus(1234, false);

  // First delta, single container ID, should get rate limited down to two containers
  ConnMap deltaSingle = {
      {conn1, statusActive},
      {conn2, statusActive},
      {conn3, statusActive},
      {conn4, statusActive},
  };

  // Second delta with two conns from two different containers - should not be
  // rate limited
  ConnMap deltaDuo = {
      {conn1, statusActive},
      {conn2, statusActive},
      {otherConn1, statusActive},
      {otherConn2, statusActive},
  };

  // final delta with two conns from containerID, along with two
  // close events. All events should be included as none of the close
  // events should be rate limited
  ConnMap deltaClose = {
      {conn1, statusActive},
      {conn2, statusActive},
      {conn3, statusClosed},
      {conn4, statusClosed},
  };

  using ConnectionsField = google::protobuf::RepeatedPtrField<sensor::NetworkConnection>;

  ConnectionsField updatesSingle;
  ConnectionsField updatesDuo;
  ConnectionsField updatesClose;

  net_status_notifier.AddConnections(&updatesSingle, deltaSingle);

  EXPECT_TRUE(updatesSingle.size() == 2);

  net_status_notifier.AddConnections(&updatesDuo, deltaDuo);

  EXPECT_TRUE(updatesDuo.size() == 4);

  net_status_notifier.AddConnections(&updatesClose, deltaClose);

  EXPECT_TRUE(updatesClose.size() == 4);
}

}  // namespace collector
