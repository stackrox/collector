#include <gmock/gmock-actions.h>
#include <gmock/gmock-spec-builders.h>

#include "CollectionMethod.h"
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
  std::string machine("x86_64");
  std::string expected_driver("collector-ebpf-5.10.0-9-cloud-amd64-gl-5.10.83-1gardenlinux1.o");
  std::string expected_path("/kernel-modules");
  KernelVersion kv(release, version, machine);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));

  auto candidate = getGardenLinuxCandidate(host);

  EXPECT_TRUE(candidate);
  EXPECT_EQ(candidate->GetName(), expected_driver);
  EXPECT_EQ(candidate->GetPath(), expected_path);
  EXPECT_EQ(candidate->GetCollectionMethod(), CollectionMethod::EBPF);
  EXPECT_TRUE(candidate->IsDownloadable());
}

TEST(getGardenLinuxCandidateTest, Garden318) {
  MockHostInfoLocal host;
  std::string release("5.4.0-6-cloud-amd64");
  std::string version("#1 SMP Debian 5.4.93-1 (2021-02-09)");
  std::string machine("x86_64");
  std::string expected_driver("collector-ebpf-5.4.0-6-cloud-amd64-gl-5.4.93-1.o");
  std::string expected_path("/kernel-modules");
  KernelVersion kv(release, version, machine);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));

  auto candidate = getGardenLinuxCandidate(host);

  EXPECT_TRUE(candidate);
  EXPECT_EQ(candidate->GetName(), expected_driver);
  EXPECT_EQ(candidate->GetPath(), expected_path);
  EXPECT_EQ(candidate->GetCollectionMethod(), CollectionMethod::EBPF);
  EXPECT_TRUE(candidate->IsDownloadable());
}

TEST(getMinikubeCandidateTest, v1_27_1) {
  MockHostInfoLocal host;
  std::string release("5.10.57");
  std::string version("#1 SMP Wed Oct 27 22:52:27 UTC 2021 x86_64 GNU/Linux");
  std::string machine("x86_64");
  std::string minikube_version("v1.27.1");
  std::string expected_driver("collector-ebpf-5.10.57-minikube-v1.27.1.o");
  std::string expected_path("/kernel-modules");
  KernelVersion kv(release, version, machine);

  EXPECT_CALL(host, GetMinikubeVersion()).WillOnce(Return(minikube_version));
  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));

  auto candidate = getMinikubeCandidate(host);

  EXPECT_TRUE(candidate);
  EXPECT_EQ(candidate->GetName(), expected_driver);
  EXPECT_EQ(candidate->GetPath(), expected_path);
  EXPECT_EQ(candidate->GetCollectionMethod(), CollectionMethod::EBPF);
  EXPECT_TRUE(candidate->IsDownloadable());
}

TEST(getMinikubeCandidateTest, v1_24_0) {
  MockHostInfoLocal host;
  std::string release("4.19.202");
  std::string version("#1 SMP Wed Oct 27 22:52:27 UTC 2021 x86_64 GNU/Linux");
  std::string machine("x86_64");
  std::string minikube_version("v1.24.0");
  std::string expected_driver("collector-ebpf-4.19.202-minikube-v1.24.0.o");
  std::string expected_path("/kernel-modules");
  KernelVersion kv(release, version, machine);

  EXPECT_CALL(host, GetMinikubeVersion()).WillOnce(Return(minikube_version));
  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));

  auto candidate = getMinikubeCandidate(host);

  EXPECT_TRUE(candidate);
  EXPECT_EQ(candidate->GetName(), expected_driver);
  EXPECT_EQ(candidate->GetPath(), expected_path);
  EXPECT_EQ(candidate->GetCollectionMethod(), CollectionMethod::EBPF);
  EXPECT_TRUE(candidate->IsDownloadable());
}

TEST(getMinikubeCandidateTest, NoVersion) {
  MockHostInfoLocal host;
  std::string release("4.19.202");
  std::string version("#1 SMP Wed Oct 27 22:52:27 UTC 2021 x86_64 GNU/Linux");
  std::string machine("x86_64");
  std::string minikube_version("");
  KernelVersion kv(release, version, machine);

  EXPECT_CALL(host, GetMinikubeVersion()).WillOnce(Return(minikube_version));

  auto ebpf_candidate = getMinikubeCandidate(host);

  EXPECT_FALSE(ebpf_candidate);
}

TEST(getUserDriverCandidate, RelativePath) {
  const char* user_input = "collector-mydriver.o";
  std::string expected_name(user_input);
  std::string expected_path("/kernel-modules");

  auto candidate = getUserDriverCandidate(user_input);

  EXPECT_EQ(candidate.GetName(), expected_name);
  EXPECT_EQ(candidate.GetPath(), expected_path);
  EXPECT_FALSE(candidate.IsDownloadable());
  EXPECT_EQ(candidate.GetCollectionMethod(), CollectionMethod::EBPF);
}

TEST(getUserDriverCandidate, FullPath) {
  const char* user_input = "/some/path/collector-mydriver.o";
  std::string expected_name("collector-mydriver.o");
  std::string expected_path("/some/path");

  auto candidate = getUserDriverCandidate(user_input);

  EXPECT_EQ(candidate.GetName(), expected_name);
  EXPECT_EQ(candidate.GetPath(), expected_path);
  EXPECT_FALSE(candidate.IsDownloadable());
  EXPECT_EQ(candidate.GetCollectionMethod(), CollectionMethod::EBPF);
}

TEST(normalizeReleaseStringTest, FedoraKernel) {
  MockHostInfoLocal host;
  std::string release("5.14.18-300.fc35.x86_64");
  std::string version("#1 SMP Fri Nov 12 16:43:17 UTC 2021");
  std::string machine("x86_64");
  std::string expected_kernel(release);
  KernelVersion kv(release, version, machine);

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
  std::string machine("x86_64");
  std::string build("build");
  std::string os_id("os");
  std::string expected_kernel("5.10.68-build-os");
  KernelVersion kv(release, version, machine);

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
  std::string machine("x86_64");
  std::string expected_kernel("5.10.47-dockerdesktop-2021-07-03-21-51-47");
  KernelVersion kv(release, version, machine);

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
  std::string machine("x86_64");
  std::string expected_kernel("5.10.0-9-cloud-amd64");
  KernelVersion kv(release, version, machine);

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
  std::string machine("x86_64");
  std::string expected_kernel("5.4.0-6-cloud-amd64");
  KernelVersion kv(release, version, machine);

  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(kv));
  EXPECT_CALL(host, IsCOS()).WillOnce(Return(false));
  EXPECT_CALL(host, IsDockerDesktop()).WillOnce(Return(false));

  auto normalized_kernel = normalizeReleaseString(host);

  EXPECT_EQ(normalized_kernel, expected_kernel);
}
}  // namespace collector
