//
// Created by Robby Cochran on 3/28/24.
//
#include <gtest/gtest.h>

#include <google/protobuf/util/json_util.h>

#include "storage/resource_collection.pb.h"

#include "Hash.h"
#include "ResourceSelector.h"

namespace collector {

class ResourceSelectorTest : public ::testing::Test {
 protected:
  storage::ResourceCollection CreateResourceCollectionFromJson(const std::string& jsonStr) {
    storage::ResourceCollection resourceCollection;
    auto status = google::protobuf::util::JsonStringToMessage(jsonStr, &resourceCollection);
    EXPECT_TRUE(status.ok()) << "Failed to parse JSON: " << status.ToString();
    return resourceCollection;
  }
};

TEST_F(ResourceSelectorTest, NamespaceIncludedWithWildcard) {
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

TEST_F(ResourceSelectorTest, NamespaceIncludedWithWildcardAnd) {
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
                        "operator": "AND",
                        "values": [
                            {
                                "value": ".*-2",
                                "matchType": "REGEX"
                            },
                            {
                                "value": "prod-.*",
                                "matchType": "REGEX"
                            }
                        ]
                    },
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
  EXPECT_FALSE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-2"));
  EXPECT_FALSE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "development"));
}

TEST_F(ResourceSelectorTest, NamespaceIncludedMultipleSelectorRules) {
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
                        "operator": "AND",
                        "values": [
                            {
                                "value": ".*2",
                                "matchType": "REGEX"
                            },
                            {
                                "value": "test-.*",
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
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "test-2"));
  EXPECT_FALSE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "test-3"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "test-32"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "development"));
}

TEST_F(ResourceSelectorTest, MultipleResourceSelectors) {
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
                                "value": "dev",
                                "matchType": "EXACT"
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
                ]
            },
            {
                "rules": [
                    {
                        "fieldName": "Cluster",
                        "operator": "OR",
                        "values": [
                            {
                                "value": "remote-.*",
                                "matchType": "REGEX"
                            }
                        ]
                    },
                    {
                        "fieldName": "Namespace",
                        "operator": "OR",
                        "values": [
                            {
                                "value": "analysis",
                                "matchType": "EXACT"
                            }
                        ]
                    },
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
  EXPECT_FALSE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, "dev", "default"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, "dev", "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, "dev", "prod-2"));
  EXPECT_FALSE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, "unknown", "prod-1"));
  EXPECT_FALSE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, "dev", "analysis"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, "remote-1", "analysis"));
  EXPECT_FALSE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, "remote-1", "prod-1"));
}

TEST_F(ResourceSelectorTest, IncludingEmbeddedCollections) {
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

  std::string jsonStr2 = R"({
        "id": "afd76230-4539-498d-abf6-6208ec5c48bb",
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
                                "value": "default",
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
        ]
    })";

  storage::ResourceCollection resourceCollection = CreateResourceCollectionFromJson(jsonStr);
  storage::ResourceCollection resourceCollection2 = CreateResourceCollectionFromJson(jsonStr2);

  UnorderedMap<std::string, storage::ResourceCollection> rcMap;

  std::string embeddedCollectionId1 = "b703d50e-b003-4a6a-bf1b-7ab36c9af184";
  std::string embeddedCollectionId2 = "afd76230-4539-498d-abf6-6208ec5c48bb";

  rcMap.insert(std::make_pair(embeddedCollectionId1, resourceCollection));
  rcMap.insert(std::make_pair(embeddedCollectionId2, resourceCollection2));

  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "default"));
  EXPECT_FALSE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "unknown"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "prod-2"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "development"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "development"));
}

}  // namespace collector
