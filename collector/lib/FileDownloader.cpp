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
#include "FileDownloader.h"

#include <unistd.h>

#include "Logging.h"

namespace collector {

static size_t WriteFile(void* content, size_t size, size_t nmemb, void* stream) {
  auto s = reinterpret_cast<std::ofstream*>(stream);
  s->write(reinterpret_cast<const char*>(content), size * nmemb);
  return size * nmemb;
}

static int DebugCallback(CURL* curl, curl_infotype type, char* data, size_t size, void* userptr) {
  // Unused arguments
  (void)curl;
  (void)userptr;
  (void)type;

  std::string msg(data, size);

  // Dump everything into the log as a first approach
  CLOG(DEBUG) << msg;

  return 0;
}

FileDownloader::FileDownloader() : connect_to_(nullptr) {
  curl_ = curl_easy_init();

  if (curl_) {
    SetDefaultOptions();
  }

  error_.fill('\0');
  retry_ = {.times = 0, .delay = 0, .max_time = std::chrono::seconds(0)};
}

FileDownloader::~FileDownloader() {
  if (curl_) {
    curl_easy_cleanup(curl_);
  }

  curl_global_cleanup();

  curl_slist_free_all(connect_to_);
}

bool FileDownloader::SetURL(const char* const url) {
  auto result = curl_easy_setopt(curl_, CURLOPT_URL, url);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set URL '" << url << "' - " << curl_easy_strerror(result);
    return false;
  }

  return true;
}

bool FileDownloader::SetURL(const std::string& url) {
  return SetURL(url.c_str());
}

void FileDownloader::IPResolve(FileDownloader::resolve_t version) {
  curl_easy_setopt(curl_, CURLOPT_IPRESOLVE, version);
}

void FileDownloader::SetRetries(unsigned int times, unsigned int delay, unsigned int max_time) {
  retry_.times = times;
  retry_.delay = delay;
  retry_.max_time = std::chrono::seconds(max_time);
}

bool FileDownloader::SetConnectionTimeout(int timeout) {
  auto result = curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, timeout);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set connection timeout - " << curl_easy_strerror(result);
    return false;
  }

  return true;
}

bool FileDownloader::FollowRedirects(bool follow) {
  auto result = curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, follow ? 1L : 0L);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set value for redirections - " << curl_easy_strerror(result);
    return false;
  }
  return true;
}

void FileDownloader::OutputFile(const std::string& path) {
  output_path_ = path;
}

void FileDownloader::OutputFile(const char* const path) {
  OutputFile(std::string(path));
}

bool FileDownloader::CACert(const char* const path) {
  if (path == nullptr) {
    CLOG(WARNING) << "CA certificate bundle path unset";
    return false;
  }

  auto result = curl_easy_setopt(curl_, CURLOPT_CAINFO, path);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set path to CA certificate bundle - " << curl_easy_strerror(result);
    return false;
  }
  return true;
}

bool FileDownloader::Cert(const char* const path) {
  if (path == nullptr) {
    CLOG(WARNING) << "SSL client certificate path unset";
    return false;
  }

  auto result = curl_easy_setopt(curl_, CURLOPT_SSLCERT, path);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set SSL client certificate - " << curl_easy_strerror(result);
    return false;
  }
  return true;
}

bool FileDownloader::Key(const char* const path) {
  if (path == nullptr) {
    CLOG(WARNING) << "SSL client key file path unset";
    return false;
  }

  auto result = curl_easy_setopt(curl_, CURLOPT_SSLKEY, path);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set key file - " << curl_easy_strerror(result);
    return false;
  }
  return true;
}

bool FileDownloader::ConnectTo(const std::string& entry) {
  return ConnectTo(entry.c_str());
}

bool FileDownloader::ConnectTo(const char* const entry) {
  curl_slist* temp = curl_slist_append(connect_to_, entry);

  if (temp == nullptr) {
    CLOG(WARNING) << "Unable to set option to connect to '" << entry;
    return false;
  }

  connect_to_ = temp;

  return true;
}

void FileDownloader::ResetCURL() {
  curl_easy_reset(curl_);

  SetDefaultOptions();

  curl_slist_free_all(connect_to_);
  connect_to_ = nullptr;
}

bool FileDownloader::IsReady() {
  return curl_ != nullptr;
}

bool FileDownloader::Download() {
  if (curl_ == nullptr) {
    CLOG(WARNING) << "Attempted to download a file using an uninitialized object";
    return false;
  }

  if (output_path_.empty()) {
    CLOG(WARNING) << "Attempted to download a file without an output set";
    return false;
  }

  if (connect_to_) {
    auto result = curl_easy_setopt(curl_, CURLOPT_CONNECT_TO, connect_to_);

    if (result != CURLE_OK) {
      CLOG(INFO) << "Unable to set connection host, the download is likely to fail - " << curl_easy_strerror(result);
    }
  }

  auto start_time = std::chrono::steady_clock::now();

  for (auto retries = retry_.times; retries; retries--) {
    std::ofstream of(output_path_, std::ios::trunc | std::ios::binary);

    if (!of.is_open()) {
      CLOG(WARNING) << "Failed to open " << output_path_;
      return false;
    }

    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&of));

    error_.fill('\0');
    auto result = curl_easy_perform(curl_);

    if (result == CURLE_OK) {
      return true;
    }

    std::string error_message(error_.data());
    CLOG(INFO) << "Fail to download " << output_path_ << " - " << (!error_message.empty() ? error_message : curl_easy_strerror(result));

    sleep(retry_.delay);

    auto time_elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time);
    if (time_elapsed > retry_.max_time) {
      CLOG(WARNING) << "Timeout while retrying to download " << output_path_;
      return false;
    }
  }

  CLOG(WARNING) << "Failed to download " << output_path_;

  return false;
}

void FileDownloader::SetDefaultOptions() {
  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteFile);
  curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, error_.data());
  curl_easy_setopt(curl_, CURLOPT_FAILONERROR, 1L);

  if (logging::GetLogLevel() <= logging::LogLevel::DEBUG) {
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl_, CURLOPT_DEBUGFUNCTION, DebugCallback);
  }

  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
}

}  // namespace collector
