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

FileDownloader::FileDownloader() : connect_to(nullptr), retry_times(0), retry_delay(0), retry_max_time(std::chrono::seconds(0)) {
  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFile);
  }
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

void FileDownloader::SetURL(const char* const url) {
  curl_easy_setopt(curl, CURLOPT_URL, url);
}

void FileDownloader::SetURL(const std::string& url) {
  SetURL(url.c_str());
}

void FileDownloader::IPResolve(FileDownloader::resolve_t version) {
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, version);
}

void FileDownloader::SetRetries(unsigned int times, unsigned int delay, unsigned int max_time) {
  retry_times = times;
  retry_delay = delay;
  retry_max_time = std::chrono::seconds(max_time);
}

void FileDownloader::SetConnectionTimeout(int timeout) {
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);
}

void FileDownloader::FollowRedirects(bool follow) {
  if (follow) {
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  } else {
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  }
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

void FileDownloader::CACert(const char* const path) {
  curl_easy_setopt(curl, CURLOPT_CAINFO, path);
}

void FileDownloader::Cert(const char* const path) {
  curl_easy_setopt(curl, CURLOPT_SSLCERT, path);
}

void FileDownloader::Key(const char* const path) {
  curl_easy_setopt(curl, CURLOPT_SSLKEY, path);
}

bool FileDownloader::ConnectTo(const char* const entry) {
  curl_slist* temp = curl_slist_append(connect_to, entry);

  if (temp == nullptr) {
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
    return false;
  }

  if (!file.is_open()) {
    CLOG(WARNING) << "Attempted to download a file without an output set";
    return false;
  }

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&file));

  if (connect_to) {
    curl_easy_setopt(curl, CURLOPT_CONNECT_TO, connect_to);
  }

  auto start_time = std::chrono::steady_clock::now();

  for (int retries = retry_times; retries; retries--) {
    auto res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
      return true;
    }

    CLOG(DEBUG) << "Fail to download file - " << curl_easy_strerror(res);

    sleep(retry_delay);

    auto time_elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time);
    if (time_elapsed > retry_max_time) {
      CLOG(WARNING) << "Timeout while retrying to download file";
      return false;
    }
  }

  CLOG(WARNING) << "Failed to download file";

  return false;
}

}  // namespace collector
