#ifndef OUTPUT_ICLIENT_H
#define OUTPUT_ICLIENT_H

#include "internalapi/sensor/collector_iservice.grpc.pb.h"

#include "SignalHandler.h"

namespace collector::output {

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
   * Recreate the internal state of the object to allow communication.
   *
   * Mostly useful for handling gRPC reconnections.
   *
   * @returns true if the refresh was succesful, false otherwise.
   */
  virtual bool Recreate() = 0;

  /**
   * Send a message to sensor through the iservice.
   *
   * @param msg The message to be sent to sensor.
   * @returns A SignalHandler::Result with the outcome of the send
   *          operation.
   */
  virtual SignalHandler::Result SendMsg(const sensor::ProcessSignal& msg) = 0;
};

}  // namespace collector::output

#endif  // OUTPUT_ICLIENT_H
