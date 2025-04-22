#ifndef COLLECTOR_GRPC_H
#define COLLECTOR_GRPC_H

#include <optional>
#include <string_view>

#include <grpcpp/channel.h>
#include <grpcpp/security/credentials.h>

#include "TlsConfig.h"
#include "optionparser.h"

namespace collector {

std::shared_ptr<grpc::ChannelCredentials> TLSCredentialsFromConfig(const TlsConfig& config);

std::shared_ptr<grpc::Channel> CreateChannel(const std::string& server_address, const std::string& hostname_override, const std::shared_ptr<grpc::ChannelCredentials>& creds);

std::pair<option::ArgStatus, std::string> CheckGrpcServer(std::string_view server);
std::pair<option::ArgStatus, std::string> CheckGrpcServer(const char* server);

}  // namespace collector

#endif  // COLLECTOR_GRPC_H
