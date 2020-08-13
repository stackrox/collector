//
// Created by Malte Isberner on 8/12/20.
//

#ifndef COLLECTOR_ENVVAR_H
#define COLLECTOR_ENVVAR_H

#include <algorithm>
#include <utility>

#include <cctype>
#include <cstdlib>

#include "Logging.h"

namespace collector {

template <typename T, typename ParseT>
class EnvVar {
 public:
  template <typename... Args>
  explicit EnvVar(const char* env_var_name, Args&& ...def_val_ctor_args) noexcept
      : env_var_name_(env_var_name), def_val_(std::forward<Args>(def_val_ctor_args)...) {}

  const T& value() const {
    static T val = [this]() {
      T v = def_val_;
      const char* env_val = std::getenv(env_var_name_);
      if (!env_val || !*env_val) {
        return v;
      }
      if (!ParseT()(&v, env_val)) {
        CLOG(WARNING) << "Failed to parse value '" << env_val << "' for environment variable " << env_var_name_ << ". Using default value.";
      }
      return v;
    }();
    return val;
  }

  explicit operator T() const {
    return value();
  }

 private:
  const char* env_var_name_;
  T def_val_;
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

}  // namespace internal

using BoolEnvVar = EnvVar<bool, internal::ParseBool>;

}  // namespace collector

#endif //COLLECTOR_ENVVAR_H
