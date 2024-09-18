#include "NamespaceSelector.h"

#include <regex>

#include "Hash.h"
#include "Logging.h"

namespace collector {

UnorderedMap<std::string, std::regex> NamespaceSelector::regexMap_;

std::regex NamespaceSelector::GetOrCompileRegex(const std::string& str) {
  auto pair = regexMap_.find(str);
  if (pair != regexMap_.end()) {
    return pair->second;
  } else {
    std::regex pattern(str);
    regexMap_[str] = pattern;
    return pattern;
  }
}

bool NamespaceSelector::IsNamespaceRuleFollowed(const storage::CollectorNamespaceConfig_NamespaceRule& nsRule, const std::string& ns) {
  if (nsRule.match_type() == storage::REGEX) {
    std::regex pattern = GetOrCompileRegex(nsRule.namespace_());
    return std::regex_match(ns, pattern);
  } else {
    return (ns == nsRule.namespace_());
  }
}

bool NamespaceSelector::IsNamespaceInSelection(const google::protobuf::RepeatedPtrField<storage::CollectorNamespaceConfig_NamespaceRule>& nsSelection, const std::string& ns) {
  if (nsSelection.size() == 0) {
    return true;
  }

  auto op = [ns](const auto& rule) -> bool {
    return IsNamespaceRuleFollowed(rule, ns);
  };

  return std::any_of(nsSelection.begin(), nsSelection.end(), op);
}

}  // namespace collector
