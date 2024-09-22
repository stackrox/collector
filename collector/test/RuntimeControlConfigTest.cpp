#include <gtest/gtest.h>

#include <google/protobuf/util/json_util.h>

#include "storage/cluster.pb.h"
#include "storage/resource_collection.pb.h"

#include "Hash.h"
#include "runtime-control/Config.h"

namespace collector {

class ConfigTest : public ::testing::Test {
 protected:
  storage::CollectorConfig CreateCollectorConfigFromJson(const std::string& jsonStr) {
    storage::CollectorConfig config;
    auto status = google::protobuf::util::JsonStringToMessage(jsonStr, &config);
    EXPECT_TRUE(status.ok()) << "Failed to parse JSON: " << status.ToString();
    return config;
  }
};

TEST_F(ConfigTest, IsFeatureEnabledTest) {
  std::string jsonStr = R"({
	"processConfig": {
	  "enabled": true
	},
	"networkEndpointConfig": {
	  "enabled": true,
	  "include_listening_endpoint_processes": true
	},
	"networkConnectionConfig": {
	  "enabled": true,
	  "aggregateExternal": true
	},
	"namespaceScopeConfig": [
	  {
	    "feature": {
	        "networkConnection": {
	          "enabled": true,
	          "aggregateExternal": false
	        }
 	    },
  	    "namespaceSelection": [
  	      {
  	        "namespace": "marketing.*",
  	        "matchType": "REGEX"
  	      },
  	      {
  	        "namespace": "namespace1",
  	        "matchType": "EXACT"
  	      }
  	    ]
  	  }
  	]
  })";

  auto msg = ConfigTest::CreateCollectorConfigFromJson(jsonStr);
  collector::runtime_control::Config::GetOrCreate().Update(msg);

  EXPECT_FALSE(collector::runtime_control::Config::GetOrCreate().IsFeatureEnabled("namespace1", "container1", 0));
  EXPECT_TRUE(collector::runtime_control::Config::GetOrCreate().IsFeatureEnabled("namespace2", "container2", 0));
  EXPECT_FALSE(collector::runtime_control::Config::GetOrCreate().IsFeatureEnabled("marketing-department", "container3", 0));
  // According to namespace it should be false but according to container it is true
  EXPECT_TRUE(collector::runtime_control::Config::GetOrCreate().IsFeatureEnabled("namespace1", "container2", 0));
}

}  // namespace collector
