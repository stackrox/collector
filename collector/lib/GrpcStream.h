#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include <grpcpp/client_context.h>
#include <grpcpp/support/client_callback.h>

#include "Utility.h"

namespace collector {

// The narrow interface used by Collector's two streaming clients.
template <typename Request>
class IStreamWriter {
 public:
  using Deadline = std::chrono::system_clock::time_point;

  virtual ~IStreamWriter() = default;
  virtual bool Write(const Request& request, Deadline deadline = Deadline::max()) = 0;
  virtual bool Sleep(Deadline deadline) = 0;
  // Stops writes, half-closes when the stream is healthy, and waits for its final status.
  virtual grpc::Status Finish(Deadline deadline) = 0;
};

// gRPC's callback reactor owns the asynchronous read/write sequencing. The
// application keeps its existing synchronous write/deadline behavior.
template <typename Request, typename Response>
class GrpcBidiStream final : public IStreamWriter<Request>,
                             public grpc::ClientBidiReactor<Request, Response> {
 public:
  using Deadline = typename IStreamWriter<Request>::Deadline;
  using Bind = std::function<void(grpc::ClientContext*, grpc::ClientBidiReactor<Request, Response>*)>;
  using OnResponse = std::function<void(const Response*)>;

  GrpcBidiStream(grpc::ClientContext* context, Bind bind, OnResponse on_response)
      : context_(context), on_response_(std::move(on_response)) {
    bind(context_, this);
    // Writes originate outside reactions, so a hold keeps OnDone from racing
    // with a write or with destruction of this reactor.
    this->AddHold();
    this->StartRead(&response_);
    this->StartCall();
  }

  ~GrpcBidiStream() override {
    context_->TryCancel();
    ReleaseHold();
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [this] { return done_; });
  }

  bool Write(const Request& request, Deadline deadline = Deadline::max()) override {
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (finishing_ || done_ || read_closed_ || write_failed_ || write_pending_) {
        return false;
      }
      write_pending_ = true;
      write_dispatching_ = true;
      write_result_ = false;
      request_ = request;  // Keep the message alive until OnWriteDone.
    }

    this->StartWrite(&request_);
    {
      std::lock_guard<std::mutex> lock(mu_);
      write_dispatching_ = false;
      cv_.notify_all();
    }
    std::unique_lock<std::mutex> lock(mu_);
    while (write_pending_ && !done_) {
      DrainResponses(lock);
      if (cv_.wait_until(lock, deadline) == std::cv_status::timeout && write_pending_) {
        lock.unlock();
        context_->TryCancel();
        return false;
      }
    }
    DrainResponses(lock);
    return write_result_;
  }

  bool Sleep(Deadline deadline) override {
    std::unique_lock<std::mutex> lock(mu_);
    for (;;) {
      DrainResponses(lock);
      if (done_ || read_closed_ || write_failed_) {
        return false;
      }
      if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
        return true;
      }
    }
  }

  grpc::Status Finish(Deadline deadline) override {
    std::unique_lock<std::mutex> lock(mu_);
    finishing_ = true;
    // Do not release the hold while an outside caller is between reserving and
    // submitting its write operation.
    cv_.wait(lock, [this] { return !write_dispatching_; });
    while (write_pending_ && !done_) {
      DrainResponses(lock);
      if (cv_.wait_until(lock, deadline) == std::cv_status::timeout && write_pending_) {
        lock.unlock();
        context_->TryCancel();
        ReleaseHold();
        return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "timed out waiting for stream write");
      }
    }

    const bool writes_done = !done_ && !write_failed_ && !initial_metadata_failed_ && !writes_done_started_;
    if (writes_done) {
      writes_done_started_ = true;
    }
    lock.unlock();
    if (writes_done) {
      this->StartWritesDone();
    }
    ReleaseHold();

    lock.lock();
    if (!cv_.wait_until(lock, deadline, [this] { return done_; })) {
      lock.unlock();
      context_->TryCancel();
      return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "timed out waiting for stream status");
    }
    DrainResponses(lock);
    return status_;
  }

 private:
  void OnReadInitialMetadataDone(bool ok) override {
    std::lock_guard<std::mutex> lock(mu_);
    initial_metadata_failed_ = !ok;
    if (!ok) {
      read_closed_ = true;
    }
    cv_.notify_all();
  }

  void OnReadDone(bool ok) override {
    if (ok) {
      if (on_response_) {
        std::lock_guard<std::mutex> lock(mu_);
        responses_.push_back(response_);
        cv_.notify_all();
      }
      this->StartRead(&response_);
    } else {
      std::lock_guard<std::mutex> lock(mu_);
      end_callback_pending_ = static_cast<bool>(on_response_);
      read_closed_ = true;
      cv_.notify_all();
    }
  }

  void DrainResponses(std::unique_lock<std::mutex>& lock) {
    while (!responses_.empty() || end_callback_pending_) {
      if (!responses_.empty()) {
        Response response = std::move(responses_.front());
        responses_.pop_front();
        lock.unlock();
        on_response_(&response);
      } else {
        end_callback_pending_ = false;
        lock.unlock();
        on_response_(nullptr);
      }
      lock.lock();
    }
  }

  void OnWriteDone(bool ok) override {
    std::lock_guard<std::mutex> lock(mu_);
    write_result_ = ok;
    write_failed_ = !ok;
    write_pending_ = false;
    cv_.notify_all();
  }

  void OnDone(const grpc::Status& status) override {
    std::lock_guard<std::mutex> lock(mu_);
    status_ = status;
    done_ = true;
    cv_.notify_all();
  }

  void ReleaseHold() {
    bool release = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (!hold_released_) {
        hold_released_ = true;
        release = true;
      }
    }
    if (release) {
      this->RemoveHold();
    }
  }

  grpc::ClientContext* context_;
  OnResponse on_response_;
  Request request_;
  Response response_;
  std::deque<Response> responses_;
  std::mutex mu_;
  std::condition_variable cv_;
  grpc::Status status_;
  bool write_dispatching_ = false;
  bool write_pending_ = false;
  bool write_result_ = false;
  bool write_failed_ = false;
  bool read_closed_ = false;
  bool end_callback_pending_ = false;
  bool initial_metadata_failed_ = false;
  bool finishing_ = false;
  bool hold_released_ = false;
  bool writes_done_started_ = false;
  bool done_ = false;
};

template <typename Request>
class StdoutStreamWriter final : public IStreamWriter<Request> {
 public:
  using Deadline = typename IStreamWriter<Request>::Deadline;

  bool Write(const Request& request, Deadline = Deadline::max()) override {
    LogProtobufMessage(request);
    return true;
  }
  bool Sleep(Deadline) override { return true; }
  grpc::Status Finish(Deadline) override { return grpc::Status::OK; }
};

}  // namespace collector
