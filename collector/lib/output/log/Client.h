#ifndef OUTPUT_LOG_CLIENT_H
#define OUTPUT_LOG_CLIENT_H

#include <google/protobuf/message.h>

#include "Utility.h"
#include "output/IClient.h"

namespace collector::output::log {
class Client : public IClient {
  SignalHandler::Result SendMsg(const MsgToSensor& msg) override {
    // This works because all variants of MsgToSensor inherit from
    // google::protobuf::Message, make sure it stays that way!
    std::visit([](const auto& v) { LogProtobufMessage(v); }, msg);
    return SignalHandler::PROCESSED;
  }

  // Always ready to send
  bool IsReady() override { return true; }
};
}  // namespace collector::output::log
#endif
