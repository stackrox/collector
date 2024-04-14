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

TEST_F(ResourceSelectorTest, NamespaceNoRuleValues) {
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
                        "values": []
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
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "default"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-2"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "development"));
}

TEST_F(ResourceSelectorTest, NoNamespace) {
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
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "default"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-2"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "development"));
}

TEST_F(ResourceSelectorTest, NoResourceSelectors) {
  std::string jsonStr = R"({
        "id": "b703d50e-b003-4a6a-bf1b-7ab36c9af184",
        "name": "External IP reporting for Ingress",
        "description": "Enable external ips on ingress",
        "createdAt": "2024-03-21T16:47:08.550364622Z",
        "lastUpdated": "2024-03-21T16:57:15.308488438Z",
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
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "default"));
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
                            }
                        ]
                    },
                    {
                        "fieldName": "Namespace",
                        "operator": "OR",
                        "values": [
                            {
                                "value": "test-.*",
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
  EXPECT_FALSE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-2"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "test-2"));
  EXPECT_FALSE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "test-3"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "test-32"));
  EXPECT_FALSE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "development"));
}

TEST_F(ResourceSelectorTest, NamespaceIncludedNoSelectorRules) {
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
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "default"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "prod-2"));
  EXPECT_TRUE(collector::ResourceSelector::IsNamespaceSelected(resourceCollection, "test-2"));
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

  EXPECT_FALSE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "default"));
  EXPECT_FALSE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "unknown"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "prod-2"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "development"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "development"));

  std::string embeddedCollectionId1 = "b703d50e-b003-4a6a-bf1b-7ab36c9af184";
  std::string embeddedCollectionId2 = "afd76230-4539-498d-abf6-6208ec5c48bb";

  rcMap.insert(std::make_pair(embeddedCollectionId1, resourceCollection));
  rcMap.insert(std::make_pair(embeddedCollectionId2, resourceCollection2));

  // Once the collection with the default namespace has been added to the ResourceCollection map it is found
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "default"));
  EXPECT_FALSE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "unknown"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "prod-1"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "prod-2"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "development"));
  EXPECT_TRUE(collector::ResourceSelector::AreClusterAndNamespaceSelected(resourceCollection, rcMap, "remote", "development"));
}

TEST_F(ResourceSelectorTest, MultipleResourceCollectionRules) {
  std::string jsonStr1 = R"({
        "id": "b703d50e-b003-4a6a-bf1b-7ab36c9af184",
        "name": "cluster-1",
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
                                "value": "cluster-1",
                                "matchType": "EXACT"
                            }
                        ]
                    }
                ]
            }
        ]
    })";

  std::string jsonStr2 = R"({
        "id": "afd76230-4539-498d-abf6-6208ec5c48bb",
        "name": "webapp and marketing",
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
    })";

  std::string jsonStr3 = R"({
        "id": "49f7deb3-3ea2-4aa3-9f40-645e9b26e2e1",
        "name": "marketing-department",
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
                            },
                        ]
                    }
                ]
            }
        ]
    })";

  auto resourceCollection1 = CreateResourceCollectionFromJson(jsonStr1);
  auto resourceCollection2 = CreateResourceCollectionFromJson(jsonStr2);
  auto resourceCollection3 = CreateResourceCollectionFromJson(jsonStr3);

  std::string collectionId1 = "b703d50e-b003-4a6a-bf1b-7ab36c9af184";
  std::string collectionId2 = "afd76230-4539-498d-abf6-6208ec5c48bb";
  std::string collectionId3 = "49f7deb3-3ea2-4aa3-9f40-645e9b26e2e1";

  UnorderedMap<std::string, storage::ResourceCollection> rcMap;

  rcMap[collectionId1] = resourceCollection1;
  rcMap[collectionId2] = resourceCollection2;
  rcMap[collectionId3] = resourceCollection3;

  FilteringRule filteringRule1(collectionId1, true);
  FilteringRule filteringRule2(collectionId2, false);
  FilteringRule filteringRule3(collectionId3, true);

  std::vector<FilteringRule> filteringRules = {filteringRule1, filteringRule2, filteringRule3};

  bool defaultStatus = false;

  EXPECT_FALSE(collector::ResourceSelector::IsFeatureEnabledForClusterAndNamespace(filteringRules, rcMap, defaultStatus, "cluster-1", "webapp"));
  EXPECT_TRUE(collector::ResourceSelector::IsFeatureEnabledForClusterAndNamespace(filteringRules, rcMap, defaultStatus, "cluster-1", "analytics"));
  EXPECT_FALSE(collector::ResourceSelector::IsFeatureEnabledForClusterAndNamespace(filteringRules, rcMap, defaultStatus, "cluster-1", "marketing-stats"));
  EXPECT_TRUE(collector::ResourceSelector::IsFeatureEnabledForClusterAndNamespace(filteringRules, rcMap, defaultStatus, "cluster-1", "marketing-department"));
  EXPECT_FALSE(collector::ResourceSelector::IsFeatureEnabledForClusterAndNamespace(filteringRules, rcMap, defaultStatus, "cluster-2", "prod"));
}

}  // namespace collector
