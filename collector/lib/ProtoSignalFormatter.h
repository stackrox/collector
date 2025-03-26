#pragma once

#include <utility>

#include <google/protobuf/message.h>

#include "ProtoAllocator.h"

// forward declarations
class sinsp_evt;
class sinsp_threadinfo;

namespace collector {

// Base class for all signal formatters that output protobuf messages.
class BaseProtoSignalFormatter {
 public:
  BaseProtoSignalFormatter() = default;

  // Returns a pointer to the proto message derived from this event, or nullptr if no message should be output.
  // Note: The caller does not assume ownership of the returned message. To avoid an additional heap allocation, the
  // implementing class should maintain an instance-level message whose address is returned by this method.
  virtual const google::protobuf::Message* ToProtoMessage(sinsp_evt* event) = 0;
  virtual const google::protobuf::Message* ToProtoMessage(sinsp_threadinfo* tinfo) = 0;
};

template <typename Message>
class ProtoSignalFormatter : public BaseProtoSignalFormatter, protected ProtoAllocator<Message> {
 public:
  ProtoSignalFormatter() = default;

  const Message* ToProtoMessage(sinsp_evt* event) override = 0;
  const Message* ToProtoMessage(sinsp_threadinfo* tinfo) override = 0;
};

}  // namespace collector
