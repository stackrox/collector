#include "ProtoUtil.h"

#include <chrono>

#include <google/protobuf/util/time_util.h>

namespace collector {

google::protobuf::Timestamp CurrentTimeProto() {
  return google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
      std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1));
}

}  // namespace collector
