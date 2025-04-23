//
// Created by Malte Isberner on 8/12/20.
//

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <optional>
#include <utility>

#include "Logging.h"
#include "Utility.h"

namespace collector {

template <typename T, typename ParseT>
class EnvVar {
 public:
  template <typename... Args>
  explicit EnvVar(const char* env_var_name, Args&&... def_val_ctor_args) noexcept
      : env_var_name_(env_var_name), val_(std::forward<Args>(def_val_ctor_args)...) {}

  explicit EnvVar(const char* env_var_name) noexcept
      : EnvVar(env_var_name, std::nullopt) {}

  bool hasValue() const {
    return tryValue().has_value();
  }

  // Will throw an exception if val_ is nullopt.
  const T& value() const {
    return tryValue().value();
  }

  const T& valueOr(const T& alt) {
    return hasValue() ? value() : alt;
  }

  const std::optional<T>& tryValue() const {
    std::call_once(init_once_, [this]() {
      const char* env_val = std::getenv(env_var_name_);
      if (env_val == nullptr) {
        return;
      }

      T val;
      if (!ParseT()(&val, env_val)) {
        CLOG(WARNING) << "Failed to parse value '" << env_val << "' for environment variable " << env_var_name_ << ". Using default value.";
      } else {
        val_ = std::make_optional(val);
      }
    });
    return val_;
  }

  explicit operator T() const {
    return value();
  }

 private:
  const char* env_var_name_;
  mutable std::optional<T> val_;
  mutable std::once_flag init_once_;
};

namespace internal {

struct ParseBool {
  bool operator()(bool* out, std::string str_val) const {
    if (str_val.empty()) {
      return false;
    }
    std::transform(str_val.begin(), str_val.end(), str_val.begin(), [](char c) -> char {
      return static_cast<char>(std::tolower(c));
    });
    *out = (str_val == "true");
    return true;
  }
};

struct ParseStringList {
  bool operator()(std::vector<std::string>* out, std::string str_val) {
    *out = SplitStringView(std::string_view(str_val), ',');
    return true;
  }
};

struct ParseString {
  bool operator()(std::string* out, std::string str_val) {
    *out = std::move(str_val);
    return true;
  }
};

struct ParseInt {
  bool operator()(int* out, const std::string& str_val) {
    *out = std::stoi(str_val);
    return true;
  }
};

struct ParsePath {
  bool operator()(std::filesystem::path* out, const std::string& str_val) {
    *out = str_val;
    return true;
  }
};

struct ParseFloat {
  bool operator()(float* out, const std::string& str_val) {
    *out = std::stof(str_val);
    return true;
  }
};
}  // namespace internal

using BoolEnvVar = EnvVar<bool, internal::ParseBool>;
using StringListEnvVar = EnvVar<std::vector<std::string>, internal::ParseStringList>;
using StringEnvVar = EnvVar<std::string, internal::ParseString>;
using IntEnvVar = EnvVar<int, internal::ParseInt>;
using PathEnvVar = EnvVar<std::filesystem::path, internal::ParsePath>;
using FloatEnvVar = EnvVar<float, internal::ParseFloat>;

}  // namespace collector
