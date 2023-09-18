#ifndef COLLECTOR_FILEDOWNLOADER_H
#define COLLECTOR_FILEDOWNLOADER_H

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

#include <curl/curl.h>

namespace collector {

struct DownloadData {
  unsigned int http_status;
  std::string error_msg;
  std::ostream* os;
};

class ConnectTo {
 public:
  ConnectTo() : connect_to_(nullptr, curl_slist_free_all) {}
  ConnectTo(std::string_view host, std::string_view connect_to);
  ConnectTo(ConnectTo&) = delete;
  ConnectTo(ConnectTo&&) = default;
  ~ConnectTo() = default;

  const curl_slist* GetList() { return connect_to_.get(); };

  const std::string& GetHost() { return host_; };
  const std::string& GetPort() { return port_; };
  const std::string& GetConnectToHost() { return connect_to_host_; };
  const std::string& GetConnectToPort() { return connect_to_port_; };

  ConnectTo& operator=(const ConnectTo& other) = delete;
  ConnectTo& operator=(ConnectTo&& other) = default;

 private:
  std::unique_ptr<curl_slist, void (*)(curl_slist*)> connect_to_;
  std::string host_;
  std::string port_;
  std::string connect_to_host_;
  std::string connect_to_port_;
};

/**
 * Wrapper aroung libcurl for downloading files.
 * See https://curl.se/libcurl/c/libcurl-easy.html for details about specific
 * methods
 */
class FileDownloader {
 public:
  enum resolve_t {
    ANY = CURL_IPRESOLVE_WHATEVER,
    IPv4 = CURL_IPRESOLVE_V4,
    IPv6 = CURL_IPRESOLVE_V6,
  };

  FileDownloader();
  ~FileDownloader();

  bool SetURL(const std::string& url);
  void IPResolve(resolve_t version);
  void SetRetries(unsigned int times, unsigned int delay, unsigned int max_time);
  bool SetConnectionTimeout(int timeout);
  bool FollowRedirects(bool follow);
  void OutputFile(const char* const path);
  void OutputFile(const std::string& path);
  bool CACert(const char* const path);
  bool Cert(const char* const path);
  bool Key(const char* const path);
  bool SetConnectTo(const std::string& host, const std::string& target);
  void SetVerboseMode(bool verbose);

  std::string GetURL() { return url_.GetURL(); }
  std::string GetHost() { return url_.GetHost(); }
  std::string GetPort() { return url_.GetPort(); }
  std::string GetScheme() { return url_.GetScheme(); }
  std::string GetPath() { return url_.GetPath(); }
  std::string GetEffectiveURL();

  void ResetCURL();
  bool IsReady();
  bool Download();

 private:
  CURL* curl_;
  std::optional<ConnectTo> connect_to_;
  std::string output_path_;
  std::string file_path_;
  std::array<char, CURL_ERROR_SIZE> error_;
  struct {
    unsigned int times;
    unsigned int delay;
    std::chrono::seconds max_time;
  } retry_;

  class URL {
   public:
    bool SetURL(const std::string& url);

    std::string GetURL() {
      auto url = scheme_ + "://" + hostname_;
      if (!port_.empty()) {
        url += ":" + port_;
      }
      return url + path_;
    }
    const std::string& GetScheme() { return scheme_; };
    const std::string& GetHost() { return hostname_; };
    const std::string& GetPort() { return port_; };
    const std::string& GetPath() { return path_; };

    void reset() {
      scheme_.clear();
      hostname_.clear();
      port_.clear();
      path_.clear();
    }

   private:
    std::string scheme_;
    std::string hostname_;
    std::string port_;
    std::string path_;
  } url_;

  void SetDefaultOptions();
  std::string GetURLPart(CURLUPart part);
};

}  // namespace collector

#endif  // COLLECTOR_FILEDOWNLOADER_H
