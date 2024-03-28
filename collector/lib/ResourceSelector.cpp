//
// Created by Robby Cochran on 3/28/24.
//

#include "ResourceSelector.h"

#include <regex>

namespace collector {

bool ResourceSelector::IsNamespaceSelected(const storage::ResourceCollection& rc, const std::string& ns) {
  for (const auto& rs : rc.resource_selectors()) {
    for (const auto& rule : rs.rules()) {
      if (rule.field_name() == "Namespace" && rule.operator_() == storage::BooleanOperator::OR) {
        for (const auto& value : rule.values()) {
          if (value.match_type() == storage::REGEX) {
            std::regex pattern(value.value());
            if (std::regex_match(ns, pattern)) {
              return true;
            }
          } else {
            if (ns == value.value()) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

}  // namespace collector