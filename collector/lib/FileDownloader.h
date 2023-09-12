#ifndef COLLECTOR_FILEDOWNLOADER_H
#define COLLECTOR_FILEDOWNLOADER_H

#include <array>
#include <chrono>
#include <ostream>

#include <curl/curl.h>
#include <curl/urlapi.h>

namespace collector {

struct DownloadData {
  unsigned int http_status;
  std::string error_msg;
  std::ostream* os;
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

  bool SetURL(const char* const url);
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
  bool ConnectTo(const std::string& entry);
  bool ConnectTo(const char* const entry);
  void SetVerboseMode(bool verbose);

  std::string GetURL() { return GetURLPart(CURLUPART_URL); }
  std::string GetHost() { return GetURLPart(CURLUPART_HOST); }
  std::string GetPort() { return GetURLPart(CURLUPART_PORT); }
  std::string GetScheme() { return GetURLPart(CURLUPART_SCHEME); }
  std::string GetPath() { return GetURLPart(CURLUPART_PATH); }
  std::string GetEffectiveURL();

  void ResetCURL();
  bool IsReady();
  bool Download();

 private:
  CURL* curl_;
  CURLU* url_;
  curl_slist* connect_to_;
  std::string output_path_;
  std::string file_path_;
  std::array<char, CURL_ERROR_SIZE> error_;
  struct {
    unsigned int times;
    unsigned int delay;
    std::chrono::seconds max_time;
  } retry_;

  void SetDefaultOptions();
  std::string GetURLPart(CURLUPart part);
};

}  // namespace collector

#endif  // COLLECTOR_FILEDOWNLOADER_H
