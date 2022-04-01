#ifndef COLLECTOR_GRPC_H
#define COLLECTOR_GRPC_H

#include <grpcpp/channel.h>
#include <grpcpp/security/credentials.h>

namespace collector {

std::shared_ptr<grpc::ChannelCredentials> TLSCredentialsFromFiles(
    const std::string& ca_cert_path, const std::string& client_cert_path, const std::string& client_key_path);

std::shared_ptr<grpc::Channel> CreateChannel(const std::string& server_address, const std::string& hostname_override, const std::shared_ptr<grpc::ChannelCredentials>& creds);

}  // namespace collector

#endif  //COLLECTOR_GRPC_H
