
#include "StdoutSignalServiceClient.h"

#include <string>

#include "Logging.h"
#include "ProtoUtil.h"

namespace collector::output {

SignalHandler::Result StdoutSignalServiceClient::PushSignals(const SignalStreamMessage& msg) {
  std::string output;
  google::protobuf::util::MessageToJsonString(msg, &output, google::protobuf::util::JsonPrintOptions{});
  CLOG(DEBUG) << "GRPC: " << output;
  return SignalHandler::PROCESSED;
}

}  // namespace collector::output
