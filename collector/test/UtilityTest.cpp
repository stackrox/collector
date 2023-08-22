#include <gmock/gmock-actions.h>
#include <gmock/gmock-spec-builders.h>

#include "Utility.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

namespace collector {

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

TEST(ExtractContainerIDFromCgroupTest, TestExtractContainerIDFromCgroup) {
  struct TestCase {
    std::string_view input;
    std::optional<std::string_view> expected_output;
  };

  TestCase cases[] = {
      {
          "/mesos/3b1cf944-1d97-40a6-ac73-b156ac2f2bfe/mesos/077d75a1-d7b6-4e55-8114-13f925fe4f49/kubepods/besteffort/pod8e18d5f1-1421-42b7-8151-fb1c3be4bd4d/e73c55f3e7f5b6a9cfc32a89bf13e44d348bcc4fa7b079f804d61fb1532ddbe5",
          "e73c55f3e7f5",
      },
      {
          "/mesos/3b1cf944-1d97-40a6-ac73-b156ac2f2bfe/kubepods/besteffort/pod8e18d5f1-1421-42b7-8151-fb1c3be4bd4d/e73c55f3e7f5b6a9cfc32a89bf13e44d348bcc4fa7b079f804d61fb1532ddbe5",
          "e73c55f3e7f5",
      },
      {
          "/docker/951e643e3c241b225b6284ef2b79a37c13fc64cbf65b5d46bda95fcb98fe63a4",
          "951e643e3c24",
      },
      {
          "/kubepods/kubepods/besteffort/pod690705f9-df6e-11e9-8dc5-025000000001/c3bfd81b7da0be97190a74a7d459f4dfa18f57c88765cde2613af112020a1c4b",
          "c3bfd81b7da0",
      },
      {
          "/kubepods/burstable/pod7cd3dba6-e475-11e9-8f99-42010a8a00d2/2bc55a8cae1704a733ba5d785d146bbed9610483380507cbf00c96b32bb637e1",
          "2bc55a8cae17",
      },
      {
          "/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-podce705797_e47e_11e9_bd71_42010a000002.slice/docker-6525e65814a99d431b6978e8f8c895013176c6c58173b56639d4b020c14e6022.scope",
          "6525e65814a9",
      },
      // podman
      {
          "/machine.slice/libpod-e9cf81e3b1d172752567c8fe6ac48c1c751b9237ab47bf55cbfd5a971a4a9519.scope/container",
          "e9cf81e3b1d1",
      },
      // KinD on podman
      {
          "/machine.slice/libpod-cbdfa0f1f08763b1963c30d98e11e1f052cb67f1e9b7c0ab8a6ca6c70cbcad69.scope/container/kubelet.slice/kubelet-kubepods.slice/kubelet-kubepods-besteffort.slice/kubelet-kubepods-besteffort-pod6eab3b7b_f0a6_4bb8_bff2_d5bc9017c04b.slice/cri-containerd-5ebf11e02dbde102cda4b76bc0e3849a65f9edac7a12bdabfd34db01b9556101.scope",
          "5ebf11e02dbd",
      },
      // conmon
      {
          "/machine.slice/libpod-conmon-b6ce30d02945df4bbf8e8b7193b2c56ebb3cd10227dd7e59d7f7cdc2cfa2a307.scope",
          {},
      },
      // host process
      {
          "/",
          {},
      },
  };

  for (const auto& c : cases) {
    auto short_container_id = ExtractContainerIDFromCgroup(c.input);
    EXPECT_EQ(short_container_id, c.expected_output);
  }
}
}  // namespace collector
