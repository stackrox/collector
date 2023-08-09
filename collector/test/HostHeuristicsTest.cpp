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

class MockS390xHeuristics : public S390XHeuristic {
 public:
  MockS390xHeuristics() = default;
};

class MockHostInfoHeuristics : public HostInfo {
 public:
  MockHostInfoHeuristics() = default;

  MOCK_METHOD0(IsUEFI, bool());
  MOCK_METHOD0(GetSecureBootStatus, SecureBootStatus());
  MOCK_METHOD0(GetKernelVersion, KernelVersion());
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
};

TEST(HostHeuristicsTest, TestS390XRHEL84) {
  MockS390xHeuristics s390xHeuristics;
  MockHostInfoHeuristics host;
  KernelVersion version = KernelVersion("4.18.0-305.88.1.el8_4.s390x", "", "s390x");
  CollectorArgs* args = CollectorArgs::getInstance();
  MockCollectorConfig config(args);
  HostConfig hconfig;

  hconfig.SetCollectionMethod(CollectionMethod::EBPF);
  EXPECT_CALL(host, GetKernelVersion()).WillOnce(Return(version));

  s390xHeuristics.Process(host, config, &hconfig);

  EXPECT_EQ(hconfig.GetCollectionMethod(), CollectionMethod::CORE_BPF);
}

}  // namespace collector
