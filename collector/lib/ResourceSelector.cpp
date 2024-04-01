//
// Created by Robby Cochran on 3/28/24.
//

#include "ResourceSelector.h"

#include <regex>

#include "Hash.h"

namespace collector {

UnorderedMap<std::string, std::regex> ResourceSelector::regexMap_;

std::regex ResourceSelector::GetOrCompileRegex(const std::string& str) {
  auto pair = regexMap_.find(str);
  if (pair != regexMap_.end()) {
    return pair->second;
  } else {
    std::regex pattern(str);
    regexMap_.insert(std::make_pair(str, pattern));
    return pattern;
  }
}

bool ResourceSelector::IsRuleValueFollowed(const storage::RuleValue& value, const std::string& resource_name) {
  if (value.match_type() == storage::REGEX) {
    std::regex pattern = GetOrCompileRegex(value.value());
    return std::regex_match(resource_name, pattern);
  } else {
    return (resource_name == value.value());
  }
}

bool ResourceSelector::IsRuleFollowed(const storage::SelectorRule& rule, const std::string& resource_name) {
  const auto& values = rule.values();

  if (values.size() == 0) {
    return true;
  }

  auto op = [resource_name](const auto& value) -> bool {
    return IsRuleValueFollowed(value, resource_name);
  };

  if (rule.operator_() == storage::BooleanOperator::OR) {
    return std::any_of(values.begin(), values.end(), op);
  } else if (rule.operator_() == storage::BooleanOperator::AND) {
    return std::all_of(values.begin(), values.end(), op);
  }

  return false;
}

bool ResourceSelector::IsResourceInResourceSelector(const storage::ResourceSelector& rs, const std::string& resource_type, const std::string& resource_name) {
  // const auto& rules = rs.rules();

  // auto op = [resource_type, resource_name](const auto& rule) -> bool {
  //   if (rule.field_name() == resource_type) {
  //     if (!IsRuleFollowed(rule, resource_name)) {
  //       return false;
  //     }
  //     return true;
  //   }
  //   return true;
  // };

  // return std::all_of(rules.begin(), rules.end(), op);

  for (const auto& rule : rs.rules()) {
    if (rule.field_name() == resource_type) {
      if (!IsRuleFollowed(rule, resource_name)) {
        return false;
      }
    }
  }

  return true;
}

bool ResourceSelector::IsNamespaceInResourceSelector(const storage::ResourceSelector& rs, const std::string& ns) {
  return IsResourceInResourceSelector(rs, "Namespace", ns);
}

bool ResourceSelector::IsClusterInResourceSelector(const storage::ResourceSelector& rs, const std::string& cluster) {
  return IsResourceInResourceSelector(rs, "Cluster", cluster);
}

bool ResourceSelector::IsNamespaceSelected(const storage::ResourceCollection& rc, const std::string& ns) {
  const auto& resource_selectors = rc.resource_selectors();

  if (resource_selectors.size() == 0) {
    return true;
  }

  auto op = [ns](const auto& rs) -> bool {
    return IsNamespaceInResourceSelector(rs, ns);
  };

  return std::any_of(resource_selectors.begin(), resource_selectors.end(), op);

  // for (const auto& rs : rc.resource_selectors()) {
  //   if (IsNamespaceInResourceSelector(rs, ns)) {
  //     return true;
  //   }
  // }
  //
  // return false;
}

bool ResourceSelector::AreClusterAndNamespaceSelected(const storage::ResourceCollection& rc, const std::string& cluster, const std::string& ns) {
  const auto& resource_selectors = rc.resource_selectors();

  auto op = [cluster, ns](const auto& rs) -> bool {
    return IsClusterInResourceSelector(rs, cluster) && IsNamespaceInResourceSelector(rs, ns);
  };

  return std::any_of(resource_selectors.begin(), resource_selectors.end(), op);
}

bool ResourceSelector::AreClusterAndNamespaceSelected(const storage::ResourceCollection& rc, const UnorderedMap<std::string, storage::ResourceCollection> rcMap, const std::string& cluster, const std::string& ns) {
  if (AreClusterAndNamespaceSelected(rc, cluster, ns)) {
    return true;
  }

  for (const auto& embeddedCollection : rc.embedded_collections()) {
    auto embeddedRc = rcMap.find(embeddedCollection.id());
    if (embeddedRc != rcMap.end()) {
      bool inEmbeddedRc = AreClusterAndNamespaceSelected(embeddedRc->second, rcMap, cluster, ns);
      if (inEmbeddedRc) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace collector
