
#include "StdoutSignalServiceClient.h"

#include <string>

#include "common/Logging.h"
#include "common/ProtoUtil.h"

namespace collector::output {

bool StdoutSignalServiceClient::PushSignals(const OutputClient::Message& msg) {
  std::string output;
  google::protobuf::util::MessageToJsonString(msg, &output, google::protobuf::util::JsonPrintOptions{});
  CLOG(DEBUG) << "GRPC: " << output;
  return true;
}

}  // namespace collector::output
