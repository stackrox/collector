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

#include <gmock/gmock-actions.h>
#include <gmock/gmock-spec-builders.h>

#include "DriverCandidates.cpp"
#include "HostInfo.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

namespace collector {

class MockHostInfoLocal : public HostInfo {
 public:
  MockHostInfoLocal() = default;

  MOCK_METHOD0(GetOSID, std::string&());
  MOCK_METHOD0(GetBuildID, std::string&());
  MOCK_METHOD0(GetKernelVersion, KernelVersion());
  MOCK_METHOD0(GetMinikubeVersion, std::string());
  MOCK_METHOD0(IsUbuntu, bool());
  MOCK_METHOD0(IsCOS, bool());
  MOCK_METHOD0(IsDockerDesktop, bool());
};

TEST(getGardenLinuxCandidateTest, Garden576_1) {
  MockHostInfoLocal host;
  std::string release("5.10.0-9-cloud-amd64");
  std::string version("#1 SMP Debian 5.10.83-1gardenlinux1 (2021-12-03)");
  std::string expected_kernel("5.10.0-9-cloud-amd64-gl-5.10.83-1gardenlinux1");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));

  auto candidate = getGardenLinuxCandidate(host);

  EXPECT_EQ(candidate, expected_kernel);
}

TEST(getGardenLinuxCandidateTest, Garden318) {
  MockHostInfoLocal host;
  std::string release("5.4.0-6-cloud-amd64");
  std::string version("#1 SMP Debian 5.4.93-1 (2021-02-09)");
  std::string expected_kernel("5.4.0-6-cloud-amd64-gl-5.4.93-1");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));

  auto candidate = getGardenLinuxCandidate(host);

  EXPECT_EQ(candidate, expected_kernel);
}

TEST(getMinikubeCandidateTest, v1_27_1) {
  MockHostInfoLocal host;
  std::string release("5.10.57");
  std::string version("#1 SMP Wed Oct 27 22:52:27 UTC 2021 x86_64 GNU/Linux");
  std::string minikube_version("v1.27.1");
  std::string expected_kernel("5.10.57-minikube-v1.27.1");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetMinikubeVersion()).WillOnce(Return(minikube_version));
  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));

  auto candidate = getMinikubeCandidate(host);

  EXPECT_EQ(candidate, expected_kernel);
}

TEST(getMinikubeCandidateTest, v1_24_0) {
  MockHostInfoLocal host;
  std::string release("4.19.202");
  std::string version("#1 SMP Wed Oct 27 22:52:27 UTC 2021 x86_64 GNU/Linux");
  std::string minikube_version("v1.24.0");
  std::string expected_kernel("4.19.202-minikube-v1.24.0");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetMinikubeVersion()).WillOnce(Return(minikube_version));
  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));

  auto candidate = getMinikubeCandidate(host);

  EXPECT_EQ(candidate, expected_kernel);
}

TEST(getMinikubeCandidateTest, NoVersion) {
  MockHostInfoLocal host;
  std::string release("4.19.202");
  std::string version("#1 SMP Wed Oct 27 22:52:27 UTC 2021 x86_64 GNU/Linux");
  std::string minikube_version("");
  std::string expected_kernel("");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetMinikubeVersion()).WillOnce(Return(minikube_version));

  auto candidate = getMinikubeCandidate(host);

  EXPECT_EQ(candidate, expected_kernel);
}

TEST(normalizeReleaseStringTest, FedoraKernel) {
  MockHostInfoLocal host;
  std::string release("5.14.18-300.fc35.x86_64");
  std::string version("#1 SMP Fri Nov 12 16:43:17 UTC 2021");
  std::string expected_kernel(release);
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));
  EXPECT_CALL(host, IsCOS()).WillOnce(Return(false));
  EXPECT_CALL(host, IsDockerDesktop()).WillOnce(Return(false));

  auto normalized_kernel = normalizeReleaseString(host);

  EXPECT_EQ(normalized_kernel, expected_kernel);
}

TEST(normalizeReleaseStringTest, COSKernel) {
  MockHostInfoLocal host;
  std::string release("5.10.68+");
  std::string version("#1 SMP Fri Dec 3 10:04:10 UTC 2021");
  std::string build("build");
  std::string os_id("os");
  std::string expected_kernel("5.10.68-build-os");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));
  EXPECT_CALL(host, IsCOS()).WillOnce(Return(true));
  EXPECT_CALL(host, GetBuildID()).WillOnce(ReturnRef(build));
  EXPECT_CALL(host, GetOSID()).WillOnce(ReturnRef(os_id));

  auto normalized_kernel = normalizeReleaseString(host);

  EXPECT_EQ(normalized_kernel, expected_kernel);
}

TEST(normalizeReleaseStringTest, DockerDesktopKernel) {
  MockHostInfoLocal host;
  std::string release("5.10.47-linuxkit");
  std::string version("#1 SMP Sat Jul 3 21:51:47 UTC 2021 x86_64 x86_64 x86_64 GNU/Linux");
  std::string expected_kernel("5.10.47-dockerdesktop-2021-07-03-21-51-47");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));
  EXPECT_CALL(host, IsCOS()).WillOnce(Return(false));
  EXPECT_CALL(host, IsDockerDesktop()).WillOnce(Return(true));

  auto normalized_kernel = normalizeReleaseString(host);

  EXPECT_EQ(normalized_kernel, expected_kernel);
}

TEST(normalizeReleaseStringTest, GardenKernel) {
  MockHostInfoLocal host;
  std::string release("5.10.0-9-cloud-amd64");
  std::string version("#1 SMP Debian 5.10.83-1gardenlinux1 (2021-12-03)");
  std::string expected_kernel("5.10.0-9-cloud-amd64");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));
  EXPECT_CALL(host, IsCOS()).WillOnce(Return(false));
  EXPECT_CALL(host, IsDockerDesktop()).WillOnce(Return(false));

  auto normalized_kernel = normalizeReleaseString(host);

  EXPECT_EQ(normalized_kernel, expected_kernel);
}

TEST(normalizeReleaseStringTest, Garden318Kernel) {
  MockHostInfoLocal host;
  std::string release("5.4.0-6-cloud-amd64");
  std::string version("#1 SMP Debian 5.4.93-1 (2021-02-09)");
  std::string expected_kernel("5.4.0-6-cloud-amd64");
  KernelVersion kv(release, version);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));
  EXPECT_CALL(host, IsCOS()).WillOnce(Return(false));
  EXPECT_CALL(host, IsDockerDesktop()).WillOnce(Return(false));

  auto normalized_kernel = normalizeReleaseString(host);

  EXPECT_EQ(normalized_kernel, expected_kernel);
}

TEST(SplitStringViewTest, TestSplitStr) {
  std::string_view view("aaaa bbbb cccc dddd");
  std::vector<std::string> splits = SplitStringView(view, ' ');
  ASSERT_EQ(4, splits.size());

  std::vector<std::string> expected = {
      "aaaa",
      "bbbb",
      "cccc",
      "dddd",
  };

  for (std::string::size_type i = 0; i < splits.size(); i++) {
    ASSERT_EQ(expected[i], splits[i]);
  }
}

TEST(SplitStringViewTest, TestSplitStrNoDelimiter) {
  std::string_view view("aaaa");
  std::vector<std::string> splits = SplitStringView(view, ' ');
  ASSERT_EQ(1, splits.size());
  ASSERT_EQ("aaaa", splits[0]);
}

TEST(SplitStringViewTest, TestSplitDelimiterAtEnd) {
  std::string_view view("a b c ");
  std::vector<std::string> expected{
      "a",
      "b",
      "c",
      "",
  };
  std::vector<std::string> splits = SplitStringView(view, ' ');
  ASSERT_EQ(expected, splits);
}

TEST(SplitStringViewTest, TestSplitDoubleDelimiter) {
  std::string_view view("a b  c");
  std::vector<std::string> expected{
      "a",
      "b",
      "",
      "c",
  };
  std::vector<std::string> splits = SplitStringView(view, ' ');
  ASSERT_EQ(expected, splits);
}
}  // namespace collector
