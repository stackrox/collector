#pragma once

#include <filesystem>
#include <utility>

class TlsConfig {
 public:
  TlsConfig(std::filesystem::path ca, std::filesystem::path clientCert, std::filesystem::path clientKey) : ca_(std::move(ca)), clientCert_(std::move(clientCert)), clientKey_(std::move(clientKey)) {}

  const std::filesystem::path& GetCa() const { return ca_; }
  const std::filesystem::path& GetClientCert() const { return clientCert_; }
  const std::filesystem::path& GetClientKey() const { return clientKey_; }

  bool IsValid() const {
    return !ca_.empty() && !clientCert_.empty() && !clientKey_.empty();
  }

 private:
  std::filesystem::path ca_;
  std::filesystem::path clientCert_;
  std::filesystem::path clientKey_;
};
