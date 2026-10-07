#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <grpcpp/create_channel.h>
#include <grpcpp/server_builder.h>

#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"
#include "internalapi/sensor/signal_iservice.grpc.pb.h"

#include "GrpcStream.h"
#include "SignalServiceClient.h"

namespace collector {
namespace {

using Request = sensor::NetworkConnectionInfoMessage;
using Response = sensor::NetworkFlowsControlMessage;
using Clock = std::chrono::system_clock;

class EchoService final : public sensor::NetworkConnectionInfoService::Service {
 public:
  grpc::Status PushNetworkConnectionInfo(grpc::ServerContext*, grpc::ServerReaderWriter<Response, Request>* stream) override {
    Request request;
    if (!stream->Read(&request)) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "missing request");
    }
    Response response;
    stream->Write(response);
    return grpc::Status::OK;
  }
};

class SignalService final : public sensor::SignalService::Service {
 public:
  grpc::Status PushSignals(grpc::ServerContext*, grpc::ServerReaderWriter<v1::Empty, sensor::SignalStreamMessage>* stream) override {
    sensor::SignalStreamMessage request;
    if (!stream->Read(&request)) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "missing signal");
    }
    return grpc::Status::OK;
  }
};

class OpenSignalService final : public sensor::SignalService::Service {
 public:
  grpc::Status PushSignals(grpc::ServerContext*, grpc::ServerReaderWriter<v1::Empty, sensor::SignalStreamMessage>* stream) override {
    sensor::SignalStreamMessage request;
    while (stream->Read(&request)) {
    }
    return grpc::Status::OK;
  }
};

class RejectingSignalService final : public sensor::SignalService::Service {
 public:
  grpc::Status PushSignals(grpc::ServerContext*, grpc::ServerReaderWriter<v1::Empty, sensor::SignalStreamMessage>*) override {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "service unavailable");
  }
};

TEST(GrpcStreamTest, CanWriteImmediatelyAfterConstruction) {
  EchoService service;
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);

  auto channel = grpc::CreateChannel("127.0.0.1:" + std::to_string(port), grpc::InsecureChannelCredentials());
  auto stub = sensor::NetworkConnectionInfoService::NewStub(channel);
  grpc::ClientContext context;
  std::atomic<int> responses{0};
  std::atomic<bool> callback_on_caller{false};
  const auto caller_thread = std::this_thread::get_id();
  {
    GrpcBidiStream<Request, Response> stream(
        &context,
        [&stub](grpc::ClientContext* ctx, grpc::ClientBidiReactor<Request, Response>* reactor) {
          stub->async()->PushNetworkConnectionInfo(ctx, reactor);
        },
        [&responses, &callback_on_caller, caller_thread](const Response* response) {
          if (response) {
            ++responses;
            callback_on_caller = std::this_thread::get_id() == caller_thread;
          }
        });
    EXPECT_TRUE(stream.Write(Request{}, Clock::now() + std::chrono::seconds(5)));
    EXPECT_FALSE(stream.Sleep(Clock::now() + std::chrono::seconds(5)));
    EXPECT_TRUE(stream.Finish(Clock::now() + std::chrono::seconds(5)).ok());
  }
  EXPECT_EQ(responses.load(), 1);
  EXPECT_TRUE(callback_on_caller.load());
  server->Shutdown();
}

TEST(GrpcStreamTest, RejectedStreamWakesSleepAndPreservesStatus) {
  RejectingSignalService service;
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);

  auto channel = grpc::CreateChannel("127.0.0.1:" + std::to_string(port), grpc::InsecureChannelCredentials());
  auto stub = sensor::SignalService::NewStub(channel);
  grpc::ClientContext context;
  {
    GrpcBidiStream<sensor::SignalStreamMessage, v1::Empty> stream(
        &context,
        [&stub](grpc::ClientContext* ctx, grpc::ClientBidiReactor<sensor::SignalStreamMessage, v1::Empty>* reactor) {
          stub->async()->PushSignals(ctx, reactor);
        },
        nullptr);
    EXPECT_FALSE(stream.Sleep(Clock::now() + std::chrono::seconds(5)));
    auto status = stream.Finish(Clock::now() + std::chrono::seconds(1));
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAVAILABLE);
  }
  server->Shutdown();
}

TEST(GrpcStreamTest, SignalStreamDoesNotRequireServerMessages) {
  SignalService service;
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);

  auto channel = grpc::CreateChannel("127.0.0.1:" + std::to_string(port), grpc::InsecureChannelCredentials());
  auto stub = sensor::SignalService::NewStub(channel);
  grpc::ClientContext context;
  {
    GrpcBidiStream<sensor::SignalStreamMessage, v1::Empty> stream(
        &context,
        [&stub](grpc::ClientContext* ctx, grpc::ClientBidiReactor<sensor::SignalStreamMessage, v1::Empty>* reactor) {
          stub->async()->PushSignals(ctx, reactor);
        },
        nullptr);
    EXPECT_TRUE(stream.Write(sensor::SignalStreamMessage{}, Clock::now() + std::chrono::seconds(5)));
    EXPECT_FALSE(stream.Sleep(Clock::now() + std::chrono::seconds(5)));
    EXPECT_TRUE(stream.Finish(Clock::now() + std::chrono::seconds(5)).ok());
  }
  server->Shutdown();
}

TEST(GrpcStreamTest, FinishHalfClosesAnOpenStream) {
  OpenSignalService service;
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);

  auto channel = grpc::CreateChannel("127.0.0.1:" + std::to_string(port), grpc::InsecureChannelCredentials());
  auto stub = sensor::SignalService::NewStub(channel);
  grpc::ClientContext context;
  {
    GrpcBidiStream<sensor::SignalStreamMessage, v1::Empty> stream(
        &context,
        [&stub](grpc::ClientContext* ctx, grpc::ClientBidiReactor<sensor::SignalStreamMessage, v1::Empty>* reactor) {
          stub->async()->PushSignals(ctx, reactor);
        },
        nullptr);
    EXPECT_TRUE(stream.Write(sensor::SignalStreamMessage{}, Clock::now() + std::chrono::seconds(5)));
    auto status = stream.Finish(Clock::now() + std::chrono::seconds(5));
    EXPECT_TRUE(status.ok()) << status.error_message();
  }
  server->Shutdown();
}

TEST(GrpcStreamTest, CancelAroundInitialWriteDoesNotAbort) {
  OpenSignalService service;
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);

  auto channel = grpc::CreateChannel("127.0.0.1:" + std::to_string(port), grpc::InsecureChannelCredentials());
  auto stub = sensor::SignalService::NewStub(channel);
  for (int i = 0; i < 100; ++i) {
    grpc::ClientContext context;
    GrpcBidiStream<sensor::SignalStreamMessage, v1::Empty> stream(
        &context,
        [&stub](grpc::ClientContext* ctx, grpc::ClientBidiReactor<sensor::SignalStreamMessage, v1::Empty>* reactor) {
          stub->async()->PushSignals(ctx, reactor);
        },
        nullptr);

    std::atomic<bool> write_entered{false};
    std::atomic<bool> write_completed{false};
    std::thread writer([&] {
      write_entered = true;
      stream.Write(sensor::SignalStreamMessage{}, Clock::now() + std::chrono::seconds(5));
      write_completed = true;
    });
    const auto entry_deadline = Clock::now() + std::chrono::seconds(1);
    while (!write_entered.load() && Clock::now() < entry_deadline) {
      std::this_thread::yield();
    }

    context.TryCancel();
    const auto status = stream.Finish(Clock::now() + std::chrono::seconds(1));
    writer.join();

    EXPECT_TRUE(write_entered.load());
    EXPECT_TRUE(write_completed.load());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::CANCELLED);
  }
  server->Shutdown();
}

TEST(GrpcStreamTest, SignalClientStopsAnOpenStream) {
  OpenSignalService service;
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);

  auto channel = grpc::CreateChannel("127.0.0.1:" + std::to_string(port), grpc::InsecureChannelCredentials());
  ASSERT_TRUE(channel->WaitForConnected(Clock::now() + std::chrono::seconds(5)));
  SignalServiceClient client(channel);
  client.Start();
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  SignalHandler::Result result = SignalHandler::ERROR;
  while (std::chrono::steady_clock::now() < deadline) {
    result = client.PushSignals(sensor::SignalStreamMessage{});
    if (result == SignalHandler::NEEDS_REFRESH) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_EQ(result, SignalHandler::NEEDS_REFRESH);
  EXPECT_EQ(client.PushSignals(sensor::SignalStreamMessage{}), SignalHandler::PROCESSED);
  client.Stop();
  server->Shutdown();
}

}  // namespace
}  // namespace collector
