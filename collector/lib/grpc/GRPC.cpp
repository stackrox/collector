#include "grpc/GRPC.h"

#include <fstream>
#include <sstream>
#include <string>

#include <grpcpp/create_channel.h>

#include "optionparser.h"

namespace collector {

namespace {

std::string ReadFileContents(const std::string& filename) {
  std::stringstream buffer;
  std::ifstream ifs(filename);
  buffer << ifs.rdbuf();
  return buffer.str();
}

}  // namespace

std::shared_ptr<grpc::ChannelCredentials> TLSCredentialsFromConfig(const TlsConfig& config) {
  grpc::SslCredentialsOptions sslOptions;

  sslOptions.pem_root_certs = ReadFileContents(config.GetCa());
  sslOptions.pem_private_key = ReadFileContents(config.GetClientKey());
  sslOptions.pem_cert_chain = ReadFileContents(config.GetClientCert());

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

std::pair<option::ArgStatus, std::string> CheckGrpcServer(std::string_view server) {
  using namespace option;

  if (server.empty()) {
    return std::make_pair(ARG_IGNORE, "Missing grpc list. Cannot configure GRPC client. Reverting to stdout.");
  }

  if (server.size() > 255) {
    return std::make_pair(ARG_ILLEGAL, "GRPC Server addr too long (> 255)");
  }

  auto marker = server.find(':');
  if (marker == std::string_view::npos) {
    return std::make_pair(ARG_ILLEGAL, "Malformed grpc server addr");
  }

  auto host = server.substr(0, marker);
  if (host.empty()) {
    return std::make_pair(ARG_ILLEGAL, "Missing grpc host");
  }

  auto port = server.substr(marker + 1);
  if (port.empty()) {
    return std::make_pair(ARG_ILLEGAL, "Missing grpc port");
  }

  return std::make_pair(ARG_OK, "");
}

std::pair<option::ArgStatus, std::string> CheckGrpcServer(const char* server) {
  if (server == nullptr) {
    return std::make_pair(option::ARG_IGNORE, "Missing grpc list. Cannot configure GRPC client. Reverting to stdout.");
  }

  return CheckGrpcServer(std::string_view(server));
}

}  // namespace collector
