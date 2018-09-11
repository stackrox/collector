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

#ifndef _GRPC_UTIL_H_
#define _GRPC_UTIL_H_

#include <grpc/grpc.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "SafeBuffer.h"

namespace collector {

// GRPCWriteRaw can be used to write a raw byte buffer to a GRPC stream.
template <typename M>
bool GRPCWriteRaw(grpc::ClientWriter<M>* writer, const SafeBuffer& buffer) {
  // TODO(mi): The buffer is already a serialized proto; this adds another (unnecessary)
  // serialization-deserialization round-trip.
  google::protobuf::io::ArrayInputStream input_stream(buffer.buffer(), buffer.size());
  M msg;
  if (!msg.ParseFromZeroCopyStream(&input_stream)) {
    CLOG_THROTTLED(ERROR, std::chrono::seconds(5)) << "Failed to send signals; Parsing failed";
    return false;
  }

  return writer->Write(msg);
}

}  // namespace collector

#endif  // _GRPC_UTIL_H_
