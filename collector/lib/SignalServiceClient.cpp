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

#include <google/protobuf/message.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace collector {

void SignalServiceClient::CreateGRPCStub(const gRPCConfig& config) {
  // Warning: When using static grpc libs, we have to explicitly call grpc_init()
  // https://github.com/grpc/grpc/issues/11366
  grpc_init();

  std::shared_ptr<grpc::ChannelCredentials> channel_creds;
  if (!config.ca_cert.empty() && !config.client_cert.empty() && !config.client_key.empty()) {
    grpc::SslCredentialsOptions sslOptions;
    sslOptions.pem_root_certs = config.ca_cert;
    sslOptions.pem_private_key = config.client_key;
    sslOptions.pem_cert_chain = config.client_cert;

    channel_creds = grpc::SslCredentials(sslOptions);
  }

  std::shared_ptr<grpc::Channel> channel;
  if (!channel_creds) {
    channel = grpc::CreateChannel(config.grpc_server.str(), grpc::InsecureChannelCredentials());
  } else {
    channel = grpc::CreateChannel(config.grpc_server.str(), channel_creds);
  }

  // Create a stub on the channel.
  stub_ = SignalService::NewStub(channel);

  grpc_writer_ = stub_->PushSignals(&context, &empty);
}

bool SignalServiceClient::PushSignals(const SafeBuffer& msg) {
  google::protobuf::io::ArrayInputStream input_stream(msg.buffer(), msg.size());
  if (!signal_stream_.ParseFromZeroCopyStream(&input_stream)) {
  	CLOG_THROTTLED(ERROR, std::chrono::seconds(5))
		<< "Failed to send signals; Parsing failed";
	return false;
  }

  if (!grpc_writer_->Write(signal_stream_)) {
    CLOG_THROTTLED(ERROR, std::chrono::seconds(5))
        << "Failed to send signals; Stream is closed";
    return false;
  }

  return true;
}

} // namespace collector
