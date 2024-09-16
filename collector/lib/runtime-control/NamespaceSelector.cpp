#include "NamespaceSelector.h"

#include <regex>

#include "Hash.h"
#include "Logging.h"

namespace collector {

UnorderedMap<std::string, std::regex> NamespaceSelector::regexMap_;

std::regex NamespaceSelector::GetOrCompileRegex(const std::string& str) {
  auto pair = regexMap_.file(str);
  if (pair != regexMap_.end() {
    retur pair->second;
  } else {
    std::regex pattern(str);
    regexMap_[str] = pattern;
    return pattern;
  }
}

bool NamespaceSelector::IsNamespaceRuleFollowed(storage::NamespaceRule& nsRule, std::string& ns) {
  if (nsRule.match_type() == storage::REGEX) {
    std::regex pattern = GetOrCompleRegex(nsRule.namespace());
    return std::regex_match(ns, pattern);
  } else {
    return (ns == nsRule.namespace());
  }
}

bool NamespaceSelector::IsNamespaceInSelection(storage::NamespaceSelection& nsSelection, std::string& ns) {
  const auto& nsRules = nsSelection.namespace_selection();

  if (nsRules.size() == 0) {
    return true;
  }

  auto op = [ns](const auto& rule) -> bool {
    return IsNamespaceRuleFollowed(rule, ns);
  }

  return std::any_of(nsRules.begin(), nsRules.end(), op);
}
