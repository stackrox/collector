//
// Created by Robby Cochran on 3/28/24.
//

#ifndef RESOURCESELECTOR_H
#define RESOURCESELECTOR_H
#include <regex>
#include <string>

#include "storage/resource_collection.pb.h"

#include "Hash.h"

namespace collector {

class ResourceSelector {
 public:
  static bool IsNamespaceSelected(const storage::ResourceCollection& rc, const std::string& ns);
  static bool AreClusterAndNamespaceSelected(const storage::ResourceCollection& rc, const std::string& cluster, const std::string& ns);
  static bool AreClusterAndNamespaceSelected(const storage::ResourceCollection& rc, const UnorderedMap<std::string, storage::ResourceCollection> rcMap, const std::string& cluster, const std::string& ns);

 private:
  static bool IsRuleFollowed(const storage::SelectorRule& rule, const std::string& ns);
  static bool IsRuleValueFollowed(const storage::RuleValue& value, const std::string& ns);
  static bool IsResourceInResourceSelector(const storage::ResourceSelector& rs, const std::string& resource_type, const std::string& resource_name);
  static bool IsNamespaceInResourceSelector(const storage::ResourceSelector& rs, const std::string& ns);
  static bool IsClusterInResourceSelector(const storage::ResourceSelector& rs, const std::string& cluster);
  static std::regex GetOrCompileRegex(const std::string& str);

  static UnorderedMap<std::string, std::regex> regexMap_;
};

}  // namespace collector

#endif  // RESOURCESELECTOR_H
