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
