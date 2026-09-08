#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

namespace collector::container_plugin {

constexpr size_t kContainerIDLength = 64;
constexpr size_t kShortContainerIDLength = 12;

inline std::optional<std::string_view> ExtractContainerIDFromCgroup(std::string_view cgroup) {
  const auto scope = cgroup.rfind(".scope");
  if (scope != std::string_view::npos) {
    cgroup.remove_suffix(cgroup.size() - scope);
  }
  if (cgroup.size() < kContainerIDLength + 1) {
    return {};
  }
  const auto id_start = cgroup.size() - kContainerIDLength;
  const char separator = cgroup[id_start - 1];
  if (separator != '/' && separator != '-' && separator != ':') {
    return {};
  }
  const std::string_view parent = cgroup.substr(0, id_start - 1);
  constexpr std::string_view kConmonSuffix = "-conmon";
  if (parent.size() >= kConmonSuffix.size() &&
      parent.substr(parent.size() - kConmonSuffix.size()) == kConmonSuffix) {
    return {};
  }
  const std::string_view id = cgroup.substr(id_start);
  if (!std::all_of(id.begin(), id.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); })) {
    return {};
  }
  return id.substr(0, kShortContainerIDLength);
}

}  // namespace collector::container_plugin
