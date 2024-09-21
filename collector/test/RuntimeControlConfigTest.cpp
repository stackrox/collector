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
  	  "clusterScopeConfig": [
  	    {
  	      "processes": {
  	        "enabled": true
  	      }
  	    },
  	    {
  	      "networkEndpoints": {
  	        "enabled": true,
  	        "include_listening_endpoint_processes": true
  	      }
  	    },
            {
  	      "networkConnections": {
  	        "enabled": true,
  	        "aggregateExternal": true
  	      }
            }
  	  ],
  	  "namespaceScopeConfig": [
  	    {
  	      "feature": {
  	        "networkConnections": {
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

  EXPECT_TRUE(collector::runtime_control::Config::GetOrCreate().IsFeatureEnabled("namespace1", "asdf1", 1));
  // EXPECT_TRUE(NamespaceSelector::IsNamespaceRuleFollowed(namespaceRule, "namespace1"));
  // EXPECT_FALSE(NamespaceSelector::IsNamespaceRuleFollowed(namespaceRule, "namespace2"));
}

}  // namespace collector
