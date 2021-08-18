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

FileDownloader::FileDownloader() : connect_to(nullptr) {
  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFile);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error.data());
  }

  error.fill('\0');
  retry = {.times = 0, .delay = 0, .max_time = std::chrono::seconds(0)};
}

FileDownloader::~FileDownloader() {
  if (curl) {
    curl_easy_cleanup(curl);
  }

  if (file.is_open()) {
    file.close();
  }

  curl_global_cleanup();

  curl_slist_free_all(connect_to);
}

bool FileDownloader::SetURL(const char* const url) {
  auto result = curl_easy_setopt(curl, CURLOPT_URL, url);

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
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, version);
}

void FileDownloader::SetRetries(unsigned int times, unsigned int delay, unsigned int max_time) {
  retry.times = times;
  retry.delay = delay;
  retry.max_time = std::chrono::seconds(max_time);
}

bool FileDownloader::SetConnectionTimeout(int timeout) {
  auto result = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set connection timeout - " << curl_easy_strerror(result);
    return false;
  }

  return true;
}

bool FileDownloader::FollowRedirects(bool follow) {
  auto result = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow ? 1L : 0L);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set value for redirections - " << curl_easy_strerror(result);
    return false;
  }
  return true;
}

void FileDownloader::OutputFile(const std::string& path) {
  if (file.is_open()) {
    file.close();
  }

  file.open(path, std::ios::binary);
}

void FileDownloader::OutputFile(const char* const path) {
  OutputFile(std::string(path));
}

bool FileDownloader::CACert(const char* const path) {
  auto result = curl_easy_setopt(curl, CURLOPT_CAINFO, path);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set path to CA certificate bundle - " << curl_easy_strerror(result);
    return false;
  }
  return true;
}

bool FileDownloader::Cert(const char* const path) {
  auto result = curl_easy_setopt(curl, CURLOPT_SSLCERT, path);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set SSL client certificate - " << curl_easy_strerror(result);
    return false;
  }
  return true;
}

bool FileDownloader::Key(const char* const path) {
  auto result = curl_easy_setopt(curl, CURLOPT_SSLKEY, path);

  if (result != CURLE_OK) {
    CLOG(WARNING) << "Unable to set key file - " << curl_easy_strerror(result);
    return false;
  }
  return true;
}

bool FileDownloader::ConnectTo(const char* const entry) {
  curl_slist* temp = curl_slist_append(connect_to, entry);

  if (temp == nullptr) {
    CLOG(WARNING) << "Unable to set option to connect to '" << entry;
    return false;
  }

  connect_to = temp;

  return true;
}

bool FileDownloader::IsReady() {
  return curl != nullptr;
}

bool FileDownloader::Download() {
  if (curl == nullptr) {
    CLOG(WARNING) << "Attempted to download a file using an uninitialized object";
    return false;
  }

  if (!file.is_open()) {
    CLOG(WARNING) << "Attempted to download a file without an output set";
    return false;
  }

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&file));

  if (connect_to) {
    auto result = curl_easy_setopt(curl, CURLOPT_CONNECT_TO, connect_to);

    if (result != CURLE_OK) {
      CLOG(INFO) << "Unable to set connection host, the download is likely to fail - " << curl_easy_strerror(result);
    }
  }

  auto start_time = std::chrono::steady_clock::now();

  for (auto retries = retry.times; retries; retries--) {
    error.fill('\0');
    auto result = curl_easy_perform(curl);

    if (result == CURLE_OK) {
      return true;
    }

    std::string error_message = error.data();
    CLOG(INFO) << "Fail to download file - " << (!error_message.empty() ? error_message : curl_easy_strerror(result));

    sleep(retry.delay);

    auto time_elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time);
    if (time_elapsed > retry.max_time) {
      CLOG(WARNING) << "Timeout while retrying to download file";
      return false;
    }
  }

  CLOG(WARNING) << "Failed to download file";

  return false;
}

}  // namespace collector
