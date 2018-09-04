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
#include "Logging.h"

#include <fstream>
#include <google/protobuf/message.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace collector {

SignalServiceClient::SignalServiceClient(const gRPCConfig& config) {
  // Warning: When using static grpc libs, we have to explicitly call grpc_init()
  // https://github.com/grpc/grpc/issues/11366
  grpc_init();

  if (!config.ca_cert.empty() && !config.client_cert.empty() && !config.client_key.empty()) {
    grpc::SslCredentialsOptions sslOptions;

    std::ifstream cafs(config.ca_cert);
    std::stringstream buffer;
    buffer << cafs.rdbuf();
    sslOptions.pem_root_certs = buffer.str();
    buffer.str(std::string());

    std::ifstream keyfs(config.client_key);
    buffer << keyfs.rdbuf();
    sslOptions.pem_private_key = buffer.str();
    buffer.str(std::string());

    std::ifstream certfs(config.client_cert);
    buffer << certfs.rdbuf();
    sslOptions.pem_cert_chain = buffer.str();

    channel_creds_ = grpc::SslCredentials(sslOptions);
  }

  grpc_server_ = config.grpc_server.str();
  channel_up_.store(false, std::memory_order_relaxed);
}

void SignalServiceClient::establishGRPCChannel() {
  const int connect_deadline = 60;
  do {
    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    channel_cond_.wait(lock, [this]() { return !channel_up_.load(std::memory_order_relaxed); });
    CLOG(INFO) << "Re-establishing GRPC channel";

    std::shared_ptr<grpc::Channel> channel;
    grpc::string key0("GRPC_ARG_KEEPALIVE_TIME_MS");

    grpc_arg arg;
    arg.type = GRPC_ARG_INTEGER;
    arg.key = const_cast<char*>(key0.c_str());
    arg.value.integer = 5000;

    grpc_channel_args channel_args;
    channel_args.num_args = 1;
    channel_args.args = &arg;

    const grpc::ChannelArguments chan_args;
    chan_args.SetChannelArgs(&channel_args);

    if (!channel_creds_) {
      CLOG(WARNING) << "GRPC channel is insecure. Use SSL option for encrypting traffic.";
      channel = grpc::CreateCustomChannel(grpc_server_, grpc::InsecureChannelCredentials(), chan_args);
    } else {
      channel = grpc::CreateCustomChannel(grpc_server_, channel_creds_, chan_args);
    }

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
    stub_.reset();
    stub_ = SignalService::NewStub(channel);

    channel_up_.store(true, std::memory_order_relaxed);
    CLOG(INFO) << "GRPC channel is established";
  } while(true);
}

void SignalServiceClient::Start() {
  thread_.Start([this]{establishGRPCChannel();});
}

bool SignalServiceClient::PushSignals(const SafeBuffer& msg) {
  if (!channel_up_.load(std::memory_order_relaxed)) {
  	CLOG_THROTTLED(ERROR, std::chrono::seconds(10))
		  << "GRPC channel is not established";
    return false;
  }

  Empty empty;
  ClientContext context;
  std::unique_ptr<ClientWriter<v1::SignalStreamMessage> > grpc_writer;
  grpc_writer.reset();
  grpc_writer = stub_->PushSignals(&context, &empty);

  google::protobuf::io::ArrayInputStream input_stream(msg.buffer(), msg.size());
  if (!signal_stream_.ParseFromZeroCopyStream(&input_stream)) {
  	CLOG_THROTTLED(ERROR, std::chrono::seconds(5))
		  << "Failed to send signals; Parsing failed";
	  return false;
  }

  if (!grpc_writer->Write(signal_stream_)) {
    Status status = grpc_writer->Finish();
    if (!status.ok()) {
      CLOG(ERROR) << "GRPC writes failed: " << status.error_message();
    }

    channel_up_.store(false, std::memory_order_relaxed);
    CLOG(ERROR) << "GRPC channel is down";
    channel_cond_.notify_one();
    return false;
  }

  return true;
}

} // namespace collector
