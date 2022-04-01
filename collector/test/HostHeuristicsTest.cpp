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

  hconfig.SetCollectionMethod("kernel-module");
  EXPECT_CALL(host, IsUEFI()).WillOnce(Return(true));
  EXPECT_CALL(host, GetSecureBootStatus())
      .WillOnce(Return(SecureBootStatus::ENABLED));
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(false));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.CollectionMethod(), "ebpf");
}

// The SecureBoot feature is undetermined and could be enabled, triggering
// switch to ebpf
TEST(HostHeuristicsTest, TestSecureBootNotDetermined) {
  MockSecureBootHeuristics secureBootHeuristics;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod("kernel-module");
  EXPECT_CALL(host, IsUEFI()).WillOnce(Return(true));
  EXPECT_CALL(host, GetSecureBootStatus())
      .WillOnce(Return(SecureBootStatus::NOT_DETERMINED));
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(false));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.CollectionMethod(), "ebpf");
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
  hconfig.SetCollectionMethod("kernel-module");
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(true));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.CollectionMethod(), "kernel-module");
}

// Booted in the legacy mode
TEST(HostHeuristicsTest, TestSecureBootLegacyBIOS) {
  MockSecureBootHeuristics secureBootHeuristics;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod("kernel-module");
  EXPECT_CALL(host, IsUEFI()).WillOnce(Return(false));
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(false));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.CollectionMethod(), "kernel-module");
}

// Garbage value is read from boot_param
TEST(HostHeuristicsTest, TestSecureBootIncorrect) {
  MockSecureBootHeuristics secureBootHeuristics;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod("kernel-module");
  EXPECT_CALL(host, IsUEFI()).WillOnce(Return(true));
  EXPECT_CALL(host, GetSecureBootStatus())
      .WillOnce(Return(static_cast<SecureBootStatus>(-1)));
  EXPECT_CALL(config, UseEbpf()).WillRepeatedly(Return(false));
  EXPECT_CALL(config, ForceKernelModules()).WillOnce(Return(false));

  secureBootHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.CollectionMethod(), "kernel-module");
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

  hconfig.SetCollectionMethod("kernel-module");
  EXPECT_CALL(host, IsGarden()).WillOnce(Return(false));

  gardenLinuxHeuristic.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.CollectionMethod(), "kernel-module");
}

TEST(GardenLinuxHeuristicsTest, UsingEBPF) {
  MockGardenLinuxHeuristic gardenLinuxHeuristic;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod("ebpf");
  EXPECT_CALL(host, IsGarden()).WillOnce(Return(true));
  EXPECT_CALL(config, UseEbpf()).WillOnce(Return(true));

  gardenLinuxHeuristic.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.CollectionMethod(), "ebpf");
}

struct GardenLinuxTestCase {
  GardenLinuxTestCase(const std::string& release, const std::string& collection_method)
      : release(release), collection_method(collection_method) {}

  std::string release;
  std::string collection_method;
};

TEST(GardenLinuxHeuristicsTest, TestReleases) {
  MockGardenLinuxHeuristic gardenLinuxHeuristic;
  MockHostInfoHeuristics host;
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  std::vector<GardenLinuxTestCase> test_cases = {
      {"Garden Linux 318.9", "kernel_module"},
      {"Garden Linux 576.2", "kernel_module"},
      {"Garden Linux 576.3", "ebpf"},
      {"Garden Linux 576.5", "ebpf"}};

  for (auto test_case : test_cases) {
    hconfig.SetCollectionMethod("kernel_module");
    EXPECT_CALL(host, IsGarden()).WillOnce(Return(true));
    EXPECT_CALL(config, UseEbpf()).WillOnce(Return(false));
    EXPECT_CALL(host, GetDistro()).WillOnce(ReturnRef(test_case.release));

    gardenLinuxHeuristic.Process(host, config, &hconfig);

    EXPECT_EQ(hconfig.CollectionMethod(), test_case.collection_method);
  }
}

}  // namespace collector
