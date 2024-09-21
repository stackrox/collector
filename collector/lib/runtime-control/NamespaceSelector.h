#ifndef NAMESPACESELECTOR_H
#define NAMESPACESELECTOR_H
#include <regex>
#include <string>

#include "storage/cluster.pb.h"

#include "Hash.h"

namespace collector {

class NamespaceSelector {
 public:
  static bool IsNamespaceRuleFollowed(const storage::CollectorNamespaceConfig_NamespaceRule& nsRule, const std::string& ns);
  static bool IsNamespaceInSelection(const google::protobuf::RepeatedPtrField<storage::CollectorNamespaceConfig_NamespaceRule>& nsSelection, const std::string& ns);

 private:
  static std::regex GetOrCompileRegex(const std::string& str);

  static UnorderedMap<std::string, std::regex> regexMap_;
};

}  // namespace collector

#endif  // NAMESPACESELECTOR_H
