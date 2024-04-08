//
// Created by Robby Cochran on 3/28/24.
//
#include <gtest/gtest.h>

#include <google/protobuf/util/json_util.h>

#include "storage/resource_collection.pb.h"

#include "ResourceSelector.h"

class NamespaceCheckerTest : public ::testing::Test {
 protected:
  storage::ResourceCollection CreateResourceCollectionFromJson(const std::string& jsonStr) {
    storage::ResourceCollection resourceCollection;
    auto status = google::protobuf::util::JsonStringToMessage(jsonStr, &resourceCollection);
    EXPECT_TRUE(status.ok()) << "Failed to parse JSON: " << status.ToString();
    return resourceCollection;
  }
};

TEST_F(NamespaceCheckerTest, NamespaceIncludedWithWildcard) {
  std::string jsonStr = R"({
        "id": "b703d50e-b003-4a6a-bf1b-7ab36c9af184",
        "name": "External IP reporting for Ingress",
        "description": "Enable external ips on ingress",
        "createdAt": "2024-03-21T16:47:08.550364622Z",
        "lastUpdated": "2024-03-21T16:57:15.308488438Z",
        "resourceSelectors": [
            {
                "rules": [
                    {
                        "fieldName": "Cluster",
                        "operator": "OR",
                        "values": [
                            {
                                "value": ".*",
                                "matchType": "REGEX"
                            }
                        ]
                    },
                    {
                        "fieldName": "Namespace",
                        "operator": "OR",
                        "values": [
                            {
                                "value": "prod-.*",
                                "matchType": "REGEX"
                            },
                            {
                                "value": "development",
                                "matchType": "EXACT"
                            }
                        ]
                    },
                    {
                        "fieldName": "Deployment",
                        "operator": "OR",
                        "values": [
                            {
                                "value": ".*",
                                "matchType": "REGEX"
                            }
                        ]
                    }
                ]
            }
        ],
        "embeddedCollections": [
            {
                "id": "afd76230-4539-498d-abf6-6208ec5c48bb"
            },
            {
                "id": "49f7deb3-3ea2-4aa3-9f40-645e9b26e2e1"
            }
        ]
    })";

  auto resourceCollection = CreateResourceCollectionFromJson(jsonStr);
  EXPECT_FALSE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "default"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-2"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "development"));
}