//
// Created by Robby Cochran on 3/28/24.
//

#ifndef RESOURCESELECTOR_H
#define RESOURCESELECTOR_H
#include <string>

#include "storage/resource_collection.pb.h"

namespace collector {

class ResourceSelector {
 public:
  static bool IsNamespaceSelected(const storage::ResourceCollection& rc, const std::string& ns);
};

}  // namespace collector

#endif  // RESOURCESELECTOR_H
