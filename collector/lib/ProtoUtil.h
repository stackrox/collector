#ifndef COLLECTOR_PROTOUTIL_H
#define COLLECTOR_PROTOUTIL_H

#include <google/protobuf/timestamp.pb.h>

namespace collector {

// Returns the current time as a timestamp proto.
// Note: the protobuf library provides a function for the same task, which however only reports the time at second
// granularity.
google::protobuf::Timestamp CurrentTimeProto();

}  // namespace collector

#endif  //COLLECTOR_PROTOUTIL_H
