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

#include "ConnScraper.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace collector {

namespace {

using CT = ConnectionTracker;
using ::testing::UnorderedElementsAre;
using ::testing::IsEmpty;

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
          "/docker/951e643e3c241b225b6284ef2b79a37c13fc64cbf65b5d46bda95fcb98fe63a4",
          "951e643e3c24",
      },
      {
          "12:pids:/kubepods/kubepods/besteffort/pod690705f9-df6e-11e9-8dc5-025000000001/c3bfd81b7da0be97190a74a7d459f4dfa18f57c88765cde2613af112020a1c4b",
          "c3bfd81b7da0",
      }
  };

  for (const auto &c : cases) {
    auto short_container_id = ExtractContainerID(c.input);
    EXPECT_EQ(short_container_id, c.exepected_output);
  }
}

}  // namespace

}  // namespace collector
