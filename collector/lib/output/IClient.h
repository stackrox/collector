#ifndef OUTPUT_ICLIENT_H
#define OUTPUT_ICLIENT_H

#include "internalapi/sensor/collector_iservice.grpc.pb.h"
#include "internalapi/sensor/signal_iservice.pb.h"

#include "SignalHandler.h"

namespace collector::output {

using MsgToSensor = std::variant<
    sensor::ProcessSignal,
    sensor::SignalStreamMessage,
    sensor::NetworkConnectionInfoMessage>;

class IClient {
 public:
  using Service = sensor::CollectorService;

  IClient() = default;
  IClient(const IClient&) = default;
  IClient(IClient&&) = delete;
  IClient& operator=(const IClient&) = default;
  IClient& operator=(IClient&&) = delete;
  virtual ~IClient() = default;

  /**
   * Send a message to sensor through the iservice.
   *
   * @param msg The message to be sent to sensor.
   * @returns A SignalHandler::Result with the outcome of the send
   *          operation.
   */
  virtual SignalHandler::Result SendMsg(const MsgToSensor& msg) = 0;

  /**
   * Check if IClient is ready to send messages.
   *
   * @returns true if IClient is ready to send, false otherwise.
   */
  virtual bool IsReady() = 0;
};

}  // namespace collector::output

#endif  // OUTPUT_ICLIENT_H
