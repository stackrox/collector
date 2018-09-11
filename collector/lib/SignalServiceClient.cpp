/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#include "SignalServiceClient.h"
#include "GRPCUtil.h"
#include "Logging.h"
#include "Utility.h"

#include <fstream>

namespace collector {

namespace {

std::string ReadFileContents(const std::string& filename) {
  std::stringstream buffer;
  std::ifstream ifs(filename);
  buffer << ifs.rdbuf();
  return buffer.str();
}

}

SignalServiceClient::SignalServiceClient(const gRPCConfig& config) {
  // Warning: When using static grpc libs, we have to explicitly call grpc_init()
  // https://github.com/grpc/grpc/issues/11366
  grpc_init();

  if (!config.ca_cert.empty() && !config.client_cert.empty() && !config.client_key.empty()) {
    grpc::SslCredentialsOptions sslOptions;

    sslOptions.pem_root_certs = ReadFileContents(config.ca_cert);
    sslOptions.pem_private_key = ReadFileContents(config.client_key);
    sslOptions.pem_cert_chain = ReadFileContents(config.client_cert);

    channel_creds_ = grpc::SslCredentials(sslOptions);
  } else {
    CLOG(WARNING) << "GRPC channel is insecure. Use SSL option for encrypting traffic.";
    channel_creds_ = grpc::InsecureChannelCredentials();
  }

  grpc_server_ = config.grpc_server.str();
  channel_up_.store(false, std::memory_order_relaxed);
}

void SignalServiceClient::establishGRPCChannel() {
  const int connect_deadline = 60;
  do {
    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    channel_cond_.wait(lock, [this]() { return !channel_up_.load(std::memory_order_acquire); });
    CLOG(INFO) << "Re-establishing GRPC channel";

    grpc::ChannelArguments chan_args;
    chan_args.SetInt("GRPC_ARG_KEEPALIVE_TIME_MS", 10000);
    chan_args.SetInt("GRPC_ARG_KEEPALIVE_TIMEOUT_MS", 10000);
    chan_args.SetInt("GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS", 1);
    chan_args.SetInt("GRPC_ARG_HTTP2_BDP_PROBE", 1);
    chan_args.SetInt("GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS", 5000);
    chan_args.SetInt("GRPC_ARG_HTTP2_MIN_SENT_PING_INTERVAL_WITHOUT_DATA_MS", 10000);
    chan_args.SetInt("GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA", 0);

    auto channel = grpc::CreateCustomChannel(grpc_server_, channel_creds_, chan_args);

    auto state = channel->GetState(true);
    while (state != GRPC_CHANNEL_CONNECTING) {
      auto connected = channel->WaitForConnected(gpr_time_add(
                          gpr_now(GPR_CLOCK_REALTIME),
                          gpr_time_from_seconds(connect_deadline, GPR_TIMESPAN)));
      if (connected) {
        CLOG(INFO) << "GRPC channel connecting";
        break;
      }
      state = channel->GetState(true);
    }

    // Create a stub on the channel.
    stub_ = SignalService::NewStub(channel);

    // stream writer
    v1::Empty empty;
    context_ = MakeUnique<grpc::ClientContext>();
    grpc_writer_ = stub_->PushSignals(context_.get(), &empty);

    channel_up_.store(true, std::memory_order_release);
    CLOG(INFO) << "GRPC channel is established";
  } while(true);
}

void SignalServiceClient::Start() {
  thread_.Start([this]{establishGRPCChannel();});
}

bool SignalServiceClient::PushSignals(const SafeBuffer& msg) {
  if (!channel_up_.load(std::memory_order_acquire)) {
  	CLOG_THROTTLED(ERROR, std::chrono::seconds(10))
		  << "GRPC channel is not established";
    return false;
  }

  if (!GRPCWriteRaw(grpc_writer_.get(), msg)) {
    Status status = grpc_writer_->Finish();
    if (!status.ok()) {
      CLOG(ERROR) << "GRPC writes failed: " << status.error_message();
    }
    context_->TryCancel();
    channel_up_.store(false, std::memory_order_release);
    CLOG(ERROR) << "GRPC channel is down";
    channel_cond_.notify_one();
    return false;
  }

  return true;
}

} // namespace collector
