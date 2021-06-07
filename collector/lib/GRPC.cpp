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

#include "GRPC.h"

#include <fstream>
#include <sstream>
#include <string>

#include <grpcpp/create_channel.h>

namespace collector {

namespace {

std::string ReadFileContents(const std::string& filename) {
  std::stringstream buffer;
  std::ifstream ifs(filename);
  buffer << ifs.rdbuf();
  return buffer.str();
}

}  // namespace

std::shared_ptr<grpc::ChannelCredentials> TLSCredentialsFromFiles(
    const std::string& ca_cert_path, const std::string& client_cert_path, const std::string& client_key_path) {
  grpc::SslCredentialsOptions sslOptions;

  sslOptions.pem_root_certs = ReadFileContents(ca_cert_path);
  sslOptions.pem_private_key = ReadFileContents(client_key_path);
  sslOptions.pem_cert_chain = ReadFileContents(client_cert_path);

  return grpc::SslCredentials(sslOptions);
}

std::shared_ptr<grpc::Channel> CreateChannel(const std::string& server_address, const std::string& hostname_override, const std::shared_ptr<grpc::ChannelCredentials>& creds) {
  grpc::ChannelArguments chan_args;
  chan_args.SetInt("GRPC_ARG_KEEPALIVE_TIME_MS", 10000);
  chan_args.SetInt("GRPC_ARG_KEEPALIVE_TIMEOUT_MS", 10000);
  chan_args.SetInt("GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS", 1);
  chan_args.SetInt("GRPC_ARG_HTTP2_BDP_PROBE", 1);
  chan_args.SetInt("GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS", 5000);
  chan_args.SetInt("GRPC_ARG_HTTP2_MIN_SENT_PING_INTERVAL_WITHOUT_DATA_MS", 10000);
  chan_args.SetInt("GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA", 0);
  if (!hostname_override.empty()) {
    chan_args.SetSslTargetNameOverride(hostname_override);
  }
  return grpc::CreateCustomChannel(server_address, creds, chan_args);
}

}  // namespace collector
