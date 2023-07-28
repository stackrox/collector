#ifndef _DRIVER_CANDIDATES_H_
#define _DRIVER_CANDIDATES_H_

#include <string>
#include <vector>

#include "CollectionMethod.h"

namespace collector {

class DriverCandidate {
 public:
  DriverCandidate(const std::string& name, CollectionMethod cm, bool downloadable = true, const std::string& path = "/kernel-modules") : name_(name), path_(path), downloadable_(downloadable), collection_method_(cm) {
  }

  inline const std::string& GetPath() const { return path_; }

  inline const std::string& GetName() const { return name_; }

  inline bool IsDownloadable() const { return downloadable_; }

  inline CollectionMethod GetCollectionMethod() const { return collection_method_; }

 private:
  std::string name_;
  std::string path_;
  bool downloadable_;
  CollectionMethod collection_method_;
};

// Get kernel candidates
std::vector<DriverCandidate> GetKernelCandidates(CollectionMethod cm);

}  // namespace collector
#endif  // _DRIVER_CANDIDATES_H_
