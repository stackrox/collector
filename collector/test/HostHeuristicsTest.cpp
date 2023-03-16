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

#include "CollectorConfig.cpp"
#include "HostConfig.h"
#include "HostHeuristics.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

namespace collector {

class MockSecureBootHeuristics : public SecureBootHeuristic {
 public:
  MockSecureBootHeuristics() = default;
};

class MockHostInfoHeuristics : public HostInfo {
 public:
  MockHostInfoHeuristics() = default;

  MOCK_METHOD0(IsUEFI, bool());
  MOCK_METHOD0(GetSecureBootStatus, SecureBootStatus());
  MOCK_METHOD0(IsGarden, bool());
  MOCK_METHOD0(GetDistro, std::string&());
};

// Note that in every test below, ProcessHostHeuristics will be called first
// with creation of a config mock, which is somewhat annoying, but doesn't
// cause any serious issues.
class MockCollectorConfig : public CollectorConfig {
 public:
  MockCollectorConfig(CollectorArgs* collectorArgs)
      : CollectorConfig(collectorArgs){};

  MOCK_CONST_METHOD0(UseEbpf, bool());
  MOCK_CONST_METHOD0(ForceKernelModules, bool());
};

// The SecureBoot feature is enabled, triggering switch to ebpf
TEST(HostHeuristicsTest, TestSecureBootEnabled) {
  MockSecureBootHeuristics secureBootHeuristics;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod(KERNEL_MODULE);
  EXPECT_CALL(host, IsUEFI()).WillOnce(Return(true));
  EXPECT_CALL(host, GetSecureBootStatus())
      .WillOnce(Return(SecureBootStatus::ENABLED));
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(false));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.GetCollectionMethod(), EBPF);
}

// The SecureBoot feature is undetermined and could be enabled, triggering
// switch to ebpf, unless we're on non-x86 architectures (Power/Z currently)
TEST(HostHeuristicsTest, TestSecureBootNotDetermined) {
  MockSecureBootHeuristics secureBootHeuristics;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod(KERNEL_MODULE);
  EXPECT_CALL(host, IsUEFI()).WillOnce(Return(true));
  EXPECT_CALL(host, GetSecureBootStatus())
      .WillOnce(Return(SecureBootStatus::NOT_DETERMINED));
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(false));

  secureBootHeuristics.Process(host, config, &hconfig);

#ifdef __x86_64__
  EXPECT_EQ(hconfig.GetCollectionMethod(), EBPF);
#else
  // for non-x86 architectures we don't modify the collection method
  // if secure boot not determined.
  EXPECT_EQ(hconfig.GetCollectionMethod(), KERNEL_MODULE);
#endif
}

// The SecureBoot feature is enabled, but the collector forced to use
// kernel-module
TEST(HostHeuristicsTest, TestSecureBootKernelModuleForced) {
  MockSecureBootHeuristics secureBootHeuristics;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  // In this constellation neither IsUEFI nor GetSecureBootStatus are called,
  // as ForceKernelModules makes the decision earlier
  hconfig.SetCollectionMethod(KERNEL_MODULE);
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(true));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.GetCollectionMethod(), KERNEL_MODULE);
}

// Booted in the legacy mode
TEST(HostHeuristicsTest, TestSecureBootLegacyBIOS) {
  MockSecureBootHeuristics secureBootHeuristics;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod(KERNEL_MODULE);
  EXPECT_CALL(host, IsUEFI()).WillOnce(Return(false));
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(false));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.GetCollectionMethod(), KERNEL_MODULE);
}

// Garbage value is read from boot_param
TEST(HostHeuristicsTest, TestSecureBootIncorrect) {
  MockSecureBootHeuristics secureBootHeuristics;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod(KERNEL_MODULE);
  EXPECT_CALL(host, IsUEFI()).WillOnce(Return(true));
  EXPECT_CALL(host, GetSecureBootStatus())
      .WillOnce(Return(static_cast<SecureBootStatus>(-1)));
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(false));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.GetCollectionMethod(), KERNEL_MODULE);
}

class MockGardenLinuxHeuristic : public GardenLinuxHeuristic {
 public:
  MockGardenLinuxHeuristic() = default;
};

TEST(GardenLinuxHeuristicsTest, NotGardenLinux) {
  MockGardenLinuxHeuristic gardenLinuxHeuristic;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod(KERNEL_MODULE);
  EXPECT_CALL(host, IsGarden()).WillOnce(Return(false));

  gardenLinuxHeuristic.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.GetCollectionMethod(), KERNEL_MODULE);
}

TEST(GardenLinuxHeuristicsTest, UsingEBPF) {
  MockGardenLinuxHeuristic gardenLinuxHeuristic;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod(EBPF);
  EXPECT_CALL(host, IsGarden()).WillOnce(Return(true));
  EXPECT_CALL(config, UseEbpf()).WillOnce(Return(true));

  gardenLinuxHeuristic.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.GetCollectionMethod(), EBPF);
}

struct GardenLinuxTestCase {
  GardenLinuxTestCase(const std::string& release, CollectionMethod collection_method)
      : release(release), collection_method(collection_method) {}

  std::string release;
  CollectionMethod collection_method;
};

TEST(GardenLinuxHeuristicsTest, TestReleases) {
  MockGardenLinuxHeuristic gardenLinuxHeuristic;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  std::vector<GardenLinuxTestCase> test_cases = {
      {"Garden Linux 318.9", KERNEL_MODULE},
      {"Garden Linux 576.2", KERNEL_MODULE},
      {"Garden Linux 576.3", EBPF},
      {"Garden Linux 576.5", EBPF}};

  for (auto test_case : test_cases) {
    hconfig.SetCollectionMethod(KERNEL_MODULE);
    EXPECT_CALL(host, IsGarden()).WillOnce(Return(true));
    EXPECT_CALL(config, UseEbpf()).WillOnce(Return(false));
    EXPECT_CALL(host, GetDistro()).WillOnce(ReturnRef(test_case.release));

    gardenLinuxHeuristic.Process(host, config, &hconfig);

    EXPECT_EQ(hconfig.GetCollectionMethod(), test_case.collection_method);
  }
}

}  // namespace collector
