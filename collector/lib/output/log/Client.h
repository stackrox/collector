#ifndef OUTPUT_LOG_CLIENT_H
#define OUTPUT_LOG_CLIENT_H

#include <google/protobuf/message.h>

#include "internalapi/sensor/collector_iservice.pb.h"

#include "Utility.h"
#include "output/IClient.h"

namespace collector::output::log {
class Client : public IClient {
  bool Recreate() override { return true; }

  SignalHandler::Result SendMsg(const sensor::MsgFromCollector& msg) override {
    LogProtobufMessage(msg);
    return SignalHandler::PROCESSED;
  }
};
}  // namespace collector::output::log
#endif
