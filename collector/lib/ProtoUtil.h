
//
// Created by Malte Isberner on 9/23/18.
//

#ifndef COLLECTOR_PROTOUTIL_H
#define COLLECTOR_PROTOUTIL_H

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "SafeBuffer.h"

namespace collector {

template <typename Msg>
Msg ProtoFromBuffer(const void* buffer, size_t size) {
  google::protobuf::io::ArrayInputStream input_stream(buffer, size);
  Msg msg;
  if (!msg.ParseFromZeroCopyStream(&input_stream)) {
    // ignore for now
  }
  return msg;
}

template <typename Msg>
Msg ProtoFromBuffer(const SafeBuffer& buf) {
  return ProtoFromBuffer<Msg>(buf.buffer(), buf.size());
}

}  // namespace collector

#endif //COLLECTOR_PROTOUTIL_H
