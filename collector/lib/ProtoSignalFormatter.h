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

  /**
   * Turn a sinsp_evt into a protobuf message to be sent to sensor.
   *
   * @param event The event to be translated.
   * @returns A pointer to the message to be sent or nullptr if no
   *          message should be sent. Ownership of the message is kept
   *          in the implementing class, caller must not attempt to free
   *          it.
   */
  const Message* ToProtoMessage(sinsp_evt* event) override = 0;

  /**
   * Turn a sinsp_threadinfo into a protobuf message to be sent to sensor.
   *
   * @param event The event to be translated.
   * @returns A pointer to the message to be sent or nullptr if no
   *          message should be sent. Ownership of the message is kept
   *          in the implementing class, caller must not attempt to free
   *          it.
   */
  const Message* ToProtoMessage(sinsp_threadinfo* tinfo) override = 0;
};

}  // namespace collector
