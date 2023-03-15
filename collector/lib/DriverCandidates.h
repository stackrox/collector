/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#ifndef _DRIVER_CANDIDATES_H_
#define _DRIVER_CANDIDATES_H_

#include <string>
#include <vector>

#include "CollectionMethod.h"

namespace collector {

class DriverCandidate {
 public:
  DriverCandidate(const std::string& name, collectionMethod cm, bool downloadable = true, const std::string& path = "/kernel-modules") : name_(name), path_(path), downloadable_(downloadable), collection_method_(cm) {
  }

  inline const std::string& GetPath() const { return path_; }

  inline const std::string& GetName() const { return name_; }

  inline bool IsDownloadable() const { return downloadable_; }

  inline collectionMethod GetCollectionMethod() const { return collection_method_; }

 private:
  std::string name_;
  std::string path_;
  bool downloadable_;
  collectionMethod collection_method_;
};

// Get kernel candidates
std::vector<DriverCandidate> GetKernelCandidates(collectionMethod cm);

}  // namespace collector
#endif  // _DRIVER_CANDIDATES_H_
