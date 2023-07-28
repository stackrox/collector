#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"

#include "ProtoAllocator.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

namespace {

TEST(ProtoAllocator, OverflowDefaultPool) {
  ProtoAllocator<sensor::NetworkConnectionInfoMessage> allocator;

  for (unsigned int i = 0; i <= ProtoAllocator<sensor::NetworkConnectionInfoMessage>::kDefaultPoolSize / sizeof(sensor::NetworkConnectionInfoMessage); i++)
    allocator.AllocateRoot();

  allocator.Reset();
}

}  // namespace

}  // namespace collector
