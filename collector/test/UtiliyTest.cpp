#include <gmock/gmock-actions.h>
#include <gmock/gmock-spec-builders.h>

#include "HostInfo.h"
#include "Utility.cpp"
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

}  // namespace collector
