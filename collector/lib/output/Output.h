#pragma once

#include "Channel.h"
#include "CollectorConfig.h"
#include "IClient.h"
#include "SignalHandler.h"

namespace collector::output {

class Output {
 public:
  Output(const Output&) = delete;
  Output(Output&&) = delete;
  Output& operator=(const Output&) = delete;
  Output& operator=(Output&&) = delete;
  ~Output() {
    // In case we have someone waiting on the client to be ready,
    // we notify them.
    client_ready_.notify_all();
  }

  Output(const CollectorConfig& config);

  // Constructor for tests
  Output(Channel<sensor::NetworkFlowsControlMessage>& ch)
      : network_control_channel_(&ch) {}
  Output(std::unique_ptr<IClient>&& sensor_client) {
    clients_.emplace_back(std::move(sensor_client));
  }

  /**
   * Send a message to sensor.
   *
   * @param msg One of sensor::MsgFromCollector or
   *            sensor::SignalStreamMessage, the proper service to be
   *            used will be determined from the type held in msg.
   * @returns A SignalHandler::Result with the outcome of the send
   *          operation
   */
  virtual SignalHandler::Result SendMsg(const MsgToSensor& msg);

  /**
   * Whether we should use the new iservice or not.
   *
   * @returns true if configuration indicates we should use the new
   *          iservice, false otherwise.
   */
  bool UseSensorClient() const { return use_sensor_client_; }

  /**
   * Retrieve the channel where the network control messages from sensor
   * are forwarded to.
   *
   * @returns A channel to retrieve control messages from.
   */
  Channel<sensor::NetworkFlowsControlMessage>& GetNetworkControlChannel() {
    return *network_control_channel_;
  }

  /**
   * Check if Output is ready to send messages by querying all its
   * clients.
   *
   * @returns true if Output is ready to send, false otherwise.
   */
  bool IsReady();

  /**
   * Wait for all clients to be ready.
   *
   * This method will block until all clients are ready to send or the
   * predicate returns true, whichever happens first.
   *
   * @param predicate A predicate for allowing unblocking in case caller
   *                  does not want to keep waiting.
   * @returns true if Output is ready, false if exited via the
   *          predicate.
   */
  bool WaitReady(const std::function<bool()>& predicate);

 private:
  std::vector<std::unique_ptr<IClient>> clients_;
  Channel<sensor::NetworkFlowsControlMessage>* network_control_channel_ = nullptr;

  bool use_sensor_client_ = true;

  std::mutex wait_mutex_;
  std::condition_variable client_ready_;
};

}  // namespace collector::output
