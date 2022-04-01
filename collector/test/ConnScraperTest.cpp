#include "ConnScraper.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

namespace {

TEST(ConnScraperTest, TestExtractContainerID) {
  struct TestCase {
    StringView input, expected_output;
  };

  TestCase cases[] = {
      {
          "11:freezer:/mesos/3b1cf944-1d97-40a6-ac73-b156ac2f2bfe/mesos/077d75a1-d7b6-4e55-8114-13f925fe4f49/kubepods/besteffort/pod8e18d5f1-1421-42b7-8151-fb1c3be4bd4d/e73c55f3e7f5b6a9cfc32a89bf13e44d348bcc4fa7b079f804d61fb1532ddbe5",
          "e73c55f3e7f5",
      },
      {
          "8:cpuacct,cpu:/mesos/3b1cf944-1d97-40a6-ac73-b156ac2f2bfe/kubepods/besteffort/pod8e18d5f1-1421-42b7-8151-fb1c3be4bd4d/e73c55f3e7f5b6a9cfc32a89bf13e44d348bcc4fa7b079f804d61fb1532ddbe5",
          "e73c55f3e7f5",
      },
      {
          "5:pids:/docker/951e643e3c241b225b6284ef2b79a37c13fc64cbf65b5d46bda95fcb98fe63a4",
          "951e643e3c24",
      },
      {
          "12:pids:/kubepods/kubepods/besteffort/pod690705f9-df6e-11e9-8dc5-025000000001/c3bfd81b7da0be97190a74a7d459f4dfa18f57c88765cde2613af112020a1c4b",
          "c3bfd81b7da0",
      },
      {
          "10:blkio:/kubepods/burstable/pod7cd3dba6-e475-11e9-8f99-42010a8a00d2/2bc55a8cae1704a733ba5d785d146bbed9610483380507cbf00c96b32bb637e1",
          "2bc55a8cae17",
      },
      {
          "6:hugetlb:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-podce705797_e47e_11e9_bd71_42010a000002.slice/docker-6525e65814a99d431b6978e8f8c895013176c6c58173b56639d4b020c14e6022.scope",
          "6525e65814a9",
      },
  };

  for (const auto& c : cases) {
    auto short_container_id = ExtractContainerID(c.input);
    EXPECT_EQ(short_container_id, c.expected_output);
  }
}

}  // namespace

}  // namespace collector
