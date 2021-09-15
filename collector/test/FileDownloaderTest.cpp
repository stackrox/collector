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
#include <sstream>

#include "gtest/gtest.h"

// Include FileDownloader.cpp in order to have direct access to static functions
#include "FileDownloader.cpp"
#include "FileDownloader.h"

TEST(FileDownloaderTest, HeaderCallbackNonHTTPHeader) {
  std::stringstream os;
  collector::DownloadData dd{.http_status = 0, .error_msg = "", .os = &os};
  char* header = const_cast<char*>("content-type: image/png\t\n");

  size_t res = collector::HeaderCallback(header, sizeof(char), strlen(header), static_cast<void*>(&dd));

  ASSERT_EQ(res, strlen(header) * sizeof(char));
  ASSERT_EQ(dd.http_status, 0);
  ASSERT_EQ(dd.error_msg, "");
}

TEST(FileDownloaderTest, HeaderCallbackUnauthorizedResponse) {
  std::stringstream os;
  collector::DownloadData dd{.http_status = 0, .error_msg = "", .os = &os};
  char* header = const_cast<char*>("HTTP/1.1 403 \t\n");

  size_t res = collector::HeaderCallback(header, sizeof(char), strlen(header), static_cast<void*>(&dd));

  ASSERT_EQ(res, strlen(header) * sizeof(char));
  ASSERT_EQ(dd.http_status, 403);
  ASSERT_EQ(dd.error_msg, "");
}

TEST(FileDownloaderTest, HeaderCallbackMalformedHTTPResponse) {
  std::stringstream os;
  collector::DownloadData dd{.http_status = 0, .error_msg = "", .os = &os};
  char* header = const_cast<char*>("HTTP/1.1403\t\n");

  size_t res = collector::HeaderCallback(header, sizeof(char), strlen(header), static_cast<void*>(&dd));

  ASSERT_EQ(res, 0);
  ASSERT_EQ(dd.http_status, 500);
  ASSERT_TRUE(dd.error_msg.find(header) != std::string::npos);
}

TEST(FileDownloaderTest, HeaderCallbackSuccessfulResponse) {
  std::stringstream os;
  collector::DownloadData dd{.http_status = 0, .error_msg = "", .os = &os};
  char* header = const_cast<char*>("HTTP/1.1 200 \t\n");

  size_t res = collector::HeaderCallback(header, sizeof(char), strlen(header), static_cast<void*>(&dd));

  ASSERT_EQ(res, strlen(header) * sizeof(char));
  ASSERT_EQ(dd.http_status, 200);
  ASSERT_EQ(dd.error_msg, "");
}

TEST(FileDownloaderTest, WriteFileSuccess) {
  std::stringstream os;
  collector::DownloadData dd{.http_status = 200, .error_msg = "", .os = &os};
  char* content = const_cast<char*>("This is some content that should be dumped into a file, for testing purposes it will be dumped to a string");

  size_t res = collector::WriteFile(content, sizeof(char), strlen(content), static_cast<void*>(&dd));

  ASSERT_EQ(res, strlen(content) * sizeof(char));
  ASSERT_EQ(os.str(), content);
}

TEST(FileDownloaderTest, WriteFileFailedRequest) {
  std::stringstream os;
  collector::DownloadData dd{.http_status = 403, .error_msg = "", .os = &os};
  char* content = const_cast<char*>("This is some content that should be dumped into a file, for testing purposes it will be dumped to a string");

  size_t res = collector::WriteFile(content, sizeof(char), strlen(content), static_cast<void*>(&dd));

  ASSERT_EQ(res, 0);
  ASSERT_EQ(os.str(), "");
  ASSERT_TRUE(dd.error_msg.find(content) != std::string::npos);
}
