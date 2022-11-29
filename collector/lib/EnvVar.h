//
// Created by Malte Isberner on 8/12/20.
//

#ifndef COLLECTOR_ENVVAR_H
#define COLLECTOR_ENVVAR_H

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <utility>

#include "Logging.h"

namespace collector {

template <typename T, typename ParseT>
class EnvVar {
 public:
  template <typename... Args>
  explicit EnvVar(const char* env_var_name, Args&&... def_val_ctor_args) noexcept
      : env_var_name_(env_var_name), val_(std::forward<Args>(def_val_ctor_args)...) {}

  const T& value() const {
    std::call_once(init_once_, [this]() {
      const char* env_val = std::getenv(env_var_name_);
      if (!env_val || !*env_val) {
        return;
      }
      if (!ParseT()(&val_, env_val)) {
        CLOG(WARNING) << "Failed to parse value '" << env_val << "' for environment variable " << env_var_name_ << ". Using default value.";
      }
    });
    return val_;
  }

  explicit operator T() const {
    return value();
  }

 private:
  const char* env_var_name_;
  mutable T val_;
  mutable std::once_flag init_once_;
};

namespace internal {

struct ParseBool {
  bool operator()(bool* out, std::string str_val) const {
    std::transform(str_val.begin(), str_val.end(), str_val.begin(), [](char c) -> char {
      return static_cast<char>(std::tolower(c));
    });
    *out = (str_val == "true");
    return true;
  }
};

struct ParseInt {
  int operator()(int* out, std::string str_val) const {
    size_t idx;
    *out = std::stoi(str_val, &idx, 0);
    return idx == str_val.length();
  }
};

}  // namespace internal

using BoolEnvVar = EnvVar<bool, internal::ParseBool>;
using IntEnvVar = EnvVar<int, internal::ParseInt>;

}  // namespace collector

#endif  // COLLECTOR_ENVVAR_H
