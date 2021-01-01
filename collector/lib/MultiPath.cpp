/*
 * Copyright (c) 2021 by StackRox Inc. All rights reserved.
 */

#include "MultiPath.h"

#include <glob.h>

#include "absl/strings/str_split.h"

namespace collector {

std::string ResolveMultiPath(const std::string& spec, bool* ok) {
  auto alternatives = absl::StrSplit(spec, ';');
  for (const auto& alt : alternatives) {
    std::string trimmed(absl::StripAsciiWhitespace(alt));
    if (trimmed.empty()) {
      continue;
    }

    glob_t glob_data;
    if (glob(trimmed.c_str(), 0, nullptr, &glob_data) == 0 && glob_data.gl_pathc > 0) {
      std::string result(glob_data.gl_pathv[glob_data.gl_pathc - 1]);
      globfree(&glob_data);
      if (ok) *ok = true;
      return result;
    }
    globfree(&glob_data);
  }

  if (ok) *ok = false;
  return spec;
}

}  // namespace collector
