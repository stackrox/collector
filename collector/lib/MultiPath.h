/*
 * Copyright (c) 2021 by StackRox Inc. All rights reserved.
 */

#ifndef _MULTI_PATH_H_
#define _MULTI_PATH_H_

#include <string>

namespace collector {

// ResolveMultiPath resolves a ";"-separated string of glob patterns by returning the *last* match
// of the *first* pattern that has a non-empty match. If none of the patterns produced a match, it returns
// the original pattern. If ok is non-null, its contents are set to whether the operation was successful
// or not.
std::string ResolveMultiPath(const std::string& spec, bool* ok = nullptr);

}  // namespace collector

#endif  // _MULTI_PATH_H_
