#include <gtest/gtest.h>

#include <google/protobuf/util/json_util.h>

#include "storage/cluster.pb.h"
#include "storage/resource_collection.pb.h"

#include "Hash.h"
#include "runtime-control/NamespaceSelector.h"

namespace collector {

class NamespaceSelectorTest : public ::testing::Test {
 protected:
  storage::CollectorNamespaceConfig_NamespaceRule CreateNamespaceRuleFromJson(const std::string& jsonStr) {
    storage::CollectorNamespaceConfig_NamespaceRule namespaceRule;
    auto status = google::protobuf::util::JsonStringToMessage(jsonStr, &namespaceRule);
    EXPECT_TRUE(status.ok()) << "Failed to parse JSON: " << status.ToString();
    return namespaceRule;
  }
};

TEST_F(NamespaceSelectorTest, IsNamespaceRuleFollowedTest) {
  std::string jsonStr = R"({
  		"namespace": "namespace1",
  		"matchType": "EXACT"
  	})";

  auto namespaceRule = NamespaceSelectorTest::CreateNamespaceRuleFromJson(jsonStr);
  EXPECT_TRUE(NamespaceSelector::IsNamespaceRuleFollowed(namespaceRule, "namespace1"));
  EXPECT_FALSE(NamespaceSelector::IsNamespaceRuleFollowed(namespaceRule, "namespace2"));
}

TEST_F(NamespaceSelectorTest, IsNamespaceRuleFollowedRegexpTest) {
  std::string jsonStr = R"({
  		"namespace": "marketing.*",
  		"matchType": "REGEX"
  	})";

  auto namespaceRule = NamespaceSelectorTest::CreateNamespaceRuleFromJson(jsonStr);
  EXPECT_TRUE(NamespaceSelector::IsNamespaceRuleFollowed(namespaceRule, "marketing-department"));
  EXPECT_FALSE(NamespaceSelector::IsNamespaceRuleFollowed(namespaceRule, "namespace2"));
}

TEST_F(NamespaceSelectorTest, IsNamespaceInSelectionTest) {
  std::string jsonStr1 = R"({
  		"namespace": "namespace1",
  		"matchType": "EXACT"
  	})";

  std::string jsonStr2 = R"({
  		"namespace": "marketing.*",
  		"matchType": "REGEX"
  	})";

  auto namespaceRule1 = NamespaceSelectorTest::CreateNamespaceRuleFromJson(jsonStr1);
  auto namespaceRule2 = NamespaceSelectorTest::CreateNamespaceRuleFromJson(jsonStr2);

  google::protobuf::RepeatedPtrField<storage::CollectorNamespaceConfig_NamespaceRule> nsSelection;
  *nsSelection.Add() = namespaceRule1;
  *nsSelection.Add() = namespaceRule2;

  EXPECT_TRUE(NamespaceSelector::IsNamespaceInSelection(nsSelection, "marketing-department"));
  EXPECT_TRUE(NamespaceSelector::IsNamespaceInSelection(nsSelection, "namespace1"));
  EXPECT_FALSE(NamespaceSelector::IsNamespaceInSelection(nsSelection, "namespace2"));
}

}  // namespace collector
