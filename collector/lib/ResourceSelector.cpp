//
// Created by Robby Cochran on 3/28/24.
//

#include "ResourceSelector.h"

#include <regex>

namespace collector {

bool ResourceSelector::IsRuleValueFollowed(const storage::RuleValue& value, const std::string& ns) {
  if (value.match_type() == storage::REGEX) {
    std::regex pattern(value.value());
    return std::regex_match(ns, pattern);
  } else {
    return (ns == value.value());
  }
}

bool ResourceSelector::IsRuleFollowed(const storage::SelectorRule& rule, const std::string& ns) {
  if (rule.operator_() == storage::BooleanOperator::OR) {
    for (const auto& value : rule.values()) {
      if (IsRuleValueFollowed(value, ns)) {
        return true;
      }
    }
  } else if (rule.operator_() == storage::BooleanOperator::AND) {
    for (const auto& value : rule.values()) {
      if (!IsRuleValueFollowed(value, ns)) {
        return false;
      }
    }
    return true;
  }

  return false;
}

bool ResourceSelector::IsResourceInResourceSelector(const storage::ResourceSelector& rs, const std::string& resource_type, const std::string& resource_name) {
  for (const auto& rule : rs.rules()) {
    if (rule.field_name() == resource_type && IsRuleFollowed(rule, resource_name)) {
      return true;
    }
  }

  return false;
}

bool ResourceSelector::IsNamespaceInResourceSelector(const storage::ResourceSelector& rs, const std::string& ns) {
  return IsResourceInResourceSelector(rs, "Namespace", ns);
}

bool ResourceSelector::IsClusterInResourceSelector(const storage::ResourceSelector& rs, const std::string& cluster) {
  return IsResourceInResourceSelector(rs, "Cluster", cluster);
}

bool ResourceSelector::IsNamespaceSelected(const storage::ResourceCollection& rc, const std::string& ns) {
  for (const auto& rs : rc.resource_selectors()) {
    if (IsNamespaceInResourceSelector(rs, ns)) {
      return true;
    }
  }

  return false;
}

bool ResourceSelector::AreClusterAndNamespaceSelected(const storage::ResourceCollection& rc, const std::string& cluster, const std::string& ns) {
  for (const auto& rs : rc.resource_selectors()) {
    if (IsClusterInResourceSelector(rs, cluster) && IsNamespaceInResourceSelector(rs, ns)) {
      return true;
    }
  }

  return false;
}

}  // namespace collector
