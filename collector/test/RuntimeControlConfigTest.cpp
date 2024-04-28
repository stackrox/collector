//
// Created by Robby Cochran on 3/28/24.
//
#include <gtest/gtest.h>

#include <google/protobuf/util/json_util.h>

#include "storage/resource_collection.pb.h"

#include "Hash.h"
// #include "ResourceSelector.h"
#include <runtime-control/Config.h>

namespace collector {

class RuntimeControlConfigTest : public ::testing::Test {
 protected:
  storage::RuntimeFilteringConfiguration CreateRuntimeFilteringConfigurationFromJson(const std::string& jsonStr) {
    storage::RuntimeFilteringConfiguration runtimeFilteringConfiguration;
    auto status = google::protobuf::util::JsonStringToMessage(jsonStr, &runtimeFilteringConfiguration);
    EXPECT_TRUE(status.ok()) << "Failed to parse JSON: " << status.ToString();
    return runtimeFilteringConfiguration;
  }
};

TEST_F(RuntimeControlConfigTest, Process) {
  std::string jsonStr = R"({
     "runtime_filters": [
       {
         "feature": "PROCESSES",
         "default_status": "off",
         "rules": [
           {
             "resource_collection_id": "b703d50e-b003-4a6a-bf1b-7ab36c9af184",
             "status": "on"
           },
           {
             "resource_collection_id": "afd76230-4539-498d-abf6-6208ec5c48bb",
             "status": "off"
           },
           {
             "resource_collection_id": "49f7deb3-3ea2-4aa3-9f40-645e9b26e2e1",
             "status": "on"
           }
         ]
       },
       {
         "feature": "EXTERNAL_IPS",
         "default_status": "on"
       },
       {
         "feature": "NETWORK_CONNECTIONS",
         "default_status": "on"
       },
       {
         "feature": "LISTENING_ENDPOINTS",
         "default_status": "on"
       }
     ],
     "resource_collections": [
       {
         "id": "b703d50e-b003-4a6a-bf1b-7ab36c9af184",
         "name": "cluster-1",
         "resourceSelectors": [
           {
             "rules": [
               {
                 "fieldName": "Cluster",
                 "operator": "OR",
                 "values": [
                   {
                     "value": "cluster-1",
                     "matchType": "EXACT"
                   }
                 ]
               }
             ]
           }
         ]
       },
       {
         "id": "afd76230-4539-498d-abf6-6208ec5c48bb",
         "name": "webapp and marketing",
         "resourceSelectors": [
           {
             "rules": [
               {
                 "fieldName": "Cluster",
                 "operator": "OR",
                 "values": [
                   {
                     "value": "cluster-1",
                     "matchType": "EXACT"
                   }
                 ]
               },
               {
                 "fieldName": "Namespace",
                 "operator": "OR",
                 "values": [
                   {
                     "value": "webapp",
                     "matchType": "EXACT"
                   },
                   {
                     "value": "marketing.*",
                     "matchType": "REGEX"
                   }
                 ]
               }
             ]
           }
         ]
       },
       {
         "id": "49f7deb3-3ea2-4aa3-9f40-645e9b26e2e1",
         "name": "marketing-department",
         "resourceSelectors": [
           {
             "rules": [
               {
                 "fieldName": "Cluster",
                 "operator": "OR",
                 "values": [
                   {
                     "value": "cluster-1",
                     "matchType": "EXACT"
                   }
                 ]
               },
               {
                 "fieldName": "Namespace",
                 "operator": "OR",
                 "values": [
                   {
                     "value": "marketing-department",
                     "matchType": "EXACT"
                   }
                 ]
               }
             ]
           }
         ]
       }
     ]
    })";

  auto runtimeFilteringConfiguration = CreateRuntimeFilteringConfigurationFromJson(jsonStr);

  runtime_control::Config::GetOrCreate().ConfigMessageToConfig(runtimeFilteringConfiguration);

  storage::RuntimeFilter_RuntimeFilterFeatures feature = storage::RuntimeFilter_RuntimeFilterFeatures_PROCESSES;

  EXPECT_FALSE(runtime_control::Config::GetOrCreate().IsFeatureEnabled("cluster-1", "marketing", "qwerty", feature));
  EXPECT_TRUE(runtime_control::Config::GetOrCreate().IsFeatureEnabled("cluster-1", "default", "asdf", feature));
  // Even though the process feature should be off for the marketing namespace it is on, because it is
  // looked up for the asdf container.
  EXPECT_TRUE(runtime_control::Config::GetOrCreate().IsFeatureEnabled("cluster-1", "marketing", "asdf", feature));
  EXPECT_TRUE(runtime_control::Config::GetOrCreate().IsFeatureEnabled("cluster-1", "marketing-department", "jklm", feature));
}

}  // namespace collector
