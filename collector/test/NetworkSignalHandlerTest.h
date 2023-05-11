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

// clang-format off
// sinsp.h needs to be included before chisel.h
#include <Utility.h>
#include "common/strlcpy.h"
#include "libsinsp/sinsp.h"
#include "libsinsp/parsers.h"
#include "chisel.h"
#include "libsinsp/wrapper.h"
#include "libsinsp/test/sinsp_with_test_input.h"
#include "libsinsp/test/test_utils.h"
// clang-format on

#include "CollectorStats.h"
#include "NetworkSignalHandler.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// class NetworkSignalHandlerTest : protected virtual sinsp_with_test_input, public virtual ::testing::Test {
// class NetworkSignalHandlerTest : protected sinsp_with_test_input, public ::testing::Test {
class NetworkSignalHandlerTest : protected sinsp_with_test_input {
 protected:
  NetworkSignalHandlerTest() {}

  void add_default_init_thread();
};
