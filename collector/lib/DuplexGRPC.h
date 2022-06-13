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

#ifndef COLLECTOR_DUPLEXGRPC_H
#define COLLECTOR_DUPLEXGRPC_H

#include <chrono>
#include <cstdint>

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/async_stream.h>

// This file defines an alternative client interface for bidirectional GRPC streams. The interface supports:
// - simultaneous reading and writing without multithreading or low-level completion queue/tag work.
// - both sync and async operations can be used at the same time.
// - all sync operations accept a deadline.
// - (limited) state checking to prevent the entire application from crashing due to a misuse of GRPC APIs.
// - poll()-style event driven interface.
//
// To instantiate a duplex client, assume that `MyService` is your service class, and `MyMethod` is the bidirectional
// streaming method. There are three options for instantiating the client:
// 1. auto client = DuplexClient::Create(&MyService::Stub::AsyncMyMethod, channel, context).
//    This allows you a client on which you can use synchronous and asynchronous reads and writes, e.g.,
//    if (client->PollAny(CAN_READ)) {
//      client->TryRead(&my_msg);   // non-blocking read, since we've polled for the event.
//      client->Write(my_response);  // blocking write
//      auto result = client->Read(&my_other_msg);  // blocking read
//    }
// 2. auto client = DuplexClient::CreateWithReadCallback(&MyService::Stub::AsyncMyMethod, channel, context, read_callback).
//    This gives a client that can only be used for writing. Read messages are passed to the specified `read_callback`,
//    which accepts a `const R*` that is non-null for any actual message that was read, and `nullptr` to indicate that
//    no more reads will happen. Note that `read_callback` is executed synchronously during event processing.
// 3. auto client = DuplexClient::CreateWithReadsIgnored(&MyService::Stub::MyAsyncMethod, channel, context);
//    This is a convenience variant of (2) with a `read_callback` that does nothing.

namespace collector {

// Internal namespace including all common definitions without polluting the surrounding namespace. Having them in a
// base class is inconvenient due to templating, which would force us to explicit import every definition from the
// templated base class via `using X = typename Base::X`.

namespace grpc_duplex_impl {

// Forward declarations
class IDuplexClient;
template <typename W>
class IDuplexClientWriter;
class DuplexClient;
template <typename W>
class DuplexClientWriter;
template <typename W, typename R>
class DuplexClientReaderWriter;

// Time-related functionality

using clock = std::chrono::system_clock;
using time_point = clock::time_point;
using duration = clock::duration;

// Timespec -> deadline conversion functions. Transparently handles `gpr_timespec`s, `std::chrono::time_point`s,
// and `std::chrono::duration`s.

template <typename TS>
inline gpr_timespec ToDeadline(const TS& time_spec) {
  return grpc::TimePoint<TS>(time_spec).raw_time();
}

template <typename Rep, typename Period>
inline gpr_timespec ToDeadline(const std::chrono::duration<Rep, Period>& timeout) {
  return ToDeadline(clock::now() + std::chrono::duration_cast<duration>(timeout));
}

inline const gpr_timespec& ToDeadline(const gpr_timespec& time_spec) {
  return time_spec;
}

// Operations that can be performed on the stream.
enum class Op : std::uint8_t {
  START = 0,
  READ,
  WRITE,
  WRITES_DONE,
  FINISH,
  SHUTDOWN,

  MAX,
};

constexpr std::size_t kNumOps = static_cast<std::size_t>(Op::MAX);

inline void* OpToTag(Op op) {
  auto ptr_val = static_cast<std::uintptr_t>(op);
  return reinterpret_cast<void*>(ptr_val);
}

inline Op TagToOp(void* tag) {
  auto ptr_val = reinterpret_cast<std::uintptr_t>(tag);
  return static_cast<Op>(ptr_val);
}

// Flags store the status of the client. Bit <op-index> is used to store that operation `op` is done, whereas bit
// <op-index> + kNumOps is used to store that operation `op` is pending. Additional statuses might be stored in the
// remaining bits.
using Flags = std::uint32_t;

inline constexpr Flags Done(Op op) {
  return static_cast<Flags>(1) << static_cast<std::uint8_t>(op);
}

inline constexpr Flags Pending(Op op) {
  return static_cast<Flags>(1) << (static_cast<std::uint8_t>(op) + kNumOps);
}

// Errors that can occur when attempting an *asynchronous* operation (i.e., the statuses do not indicate whether the
// operation *completed* successfully).
enum class OpError : std::uint8_t {
  OK = 0,           // Async operation could be performed.
  ALREADY_PENDING,  // The operation is already pending.
  ALREADY_DONE,     // The operation was already done. Idempotent operations only.
  ILLEGAL_STATE,    // The operation couldn't be performed due to the current state of the client.
  SHUTDOWN,         // The client was shutdown.
};

// Describes the outcome of starting an asynchronous operation.
struct OpDescriptor {
  Op op;             // The operation that was requested.
  OpError op_error;  // The error when attempting the asynchronous operation, if any.
};

// Describes the outcome of *completing* an operation.
struct OpResult {
  Op op;
  bool ok;
};

// Helper struct to determine if a parameter pack is empty. We use this to determine whether an operation is idempotent.
template <typename... Args>
struct is_empty : std::false_type {};
template <>
struct is_empty<> : std::true_type {};

// Status codes
enum class Status {
  OK,               // Operation started at GRPC level (async) / completed successfully (sync).
  ALREADY_PENDING,  // Operation is already pending.
  ALREADY_DONE,     // Operation was already done (idempotent operations only).

  ERROR,          // The operation failed (sync only).
  TIMEOUT,        // Timed out waiting for the desired condition (sync only).
  ILLEGAL_STATE,  // The operation is not valid in the current state.
  SHUTDOWN,       // The client was shutdown.

  INTERNAL_ERROR,  // An internal error in the duplex client library.
};

// Convenience wrapper around a `Status`.
class Result final {
 public:
  explicit operator bool() const {
    return ok();
  }

  // Checks if the operation was successful.
  bool ok() const {
    return status_ == Status::OK;
  }

  // Checks if the requested operation was done (not necessarily in this call).
  bool done() const {
    return status_ == Status::OK || status_ == Status::ALREADY_DONE;
  }

  // Checks if the operation timed out.
  bool IsTimeout() const {
    return status_ == Status::TIMEOUT;
  }

 private:
  explicit Result(bool ok) : status_(ok ? Status::OK : Status::ERROR) {}
  explicit Result(Status status) : status_(status) {}
  explicit Result(grpc::CompletionQueue::NextStatus status) {
    switch (status) {
      case grpc::CompletionQueue::SHUTDOWN:
        status_ = Status::SHUTDOWN;
        break;
      case grpc::CompletionQueue::TIMEOUT:
        status_ = Status::TIMEOUT;
        break;
      case grpc::CompletionQueue::GOT_EVENT:
        status_ = Status::OK;
        break;
      default:
        status_ = Status::INTERNAL_ERROR;
        break;
    }
  }
  explicit Result(OpError op_error) {
    switch (op_error) {
      case OpError::OK:
        status_ = Status::OK;
        break;
      case OpError::ALREADY_PENDING:
        status_ = Status::ALREADY_PENDING;
        break;
      case OpError::ALREADY_DONE:
        status_ = Status::ALREADY_DONE;
        break;
      case OpError::ILLEGAL_STATE:
        status_ = Status::ILLEGAL_STATE;
        break;
      case OpError::SHUTDOWN:
        status_ = Status::SHUTDOWN;
        break;
      default:
        status_ = Status::INTERNAL_ERROR;
        break;
    }
  }
  explicit Result(OpDescriptor op_descr) : Result(op_descr.op_error) {}

  Status status_;

  friend class DuplexClient;
  template <typename W>
  friend class DuplexClientWriter;
  template <typename W, typename R>
  friend class DuplexClientReaderWriter;
};

class IDuplexClient {
 public:
  virtual ~IDuplexClient() {}

  virtual Result WaitUntilStarted(const gpr_timespec& deadline) = 0;
  virtual bool Sleep(const gpr_timespec& deadline) = 0;
  virtual Result WritesDoneAsync() = 0;
  virtual Result WritesDone(const gpr_timespec& deadline) = 0;
  virtual Result FinishAsync() = 0;
  virtual Result WaitUntilFinished(const gpr_timespec& deadline) = 0;
  virtual Result Finish(grpc::Status* status, const gpr_timespec& deadline) = 0;
  virtual grpc::Status Finish(const gpr_timespec& deadline) = 0;
  virtual void TryCancel() = 0;
  virtual Result Shutdown() = 0;

  // Templated and utility methods

  // Wait for the specified time for the stream to become ready.
  template <typename TS = time_point>
  Result WaitUntilStarted(const TS& time_spec = time_point::max()) {
    return WaitUntilStarted(ToDeadline(time_spec));
  }

  // Waits until the given time, or until an error in the stream occurs. A return value of true indicates that no
  // error occurred during the given time.
  template <typename TS = time_point>
  bool Sleep(const TS& time_spec = time_point::max()) {
    return Sleep(ToDeadline(time_spec));
  }

  template <typename TS = time_point>
  Result WritesDone(const TS& time_spec = time_point::max()) {
    return WritesDone(ToDeadline(time_spec));
  }

  template <typename TS = time_point>
  Result WaitUntilFinished(const TS& time_spec = time_point::max()) {
    return WaitUntilFinished(ToDeadline(time_spec));
  }

  template <typename TS = time_point>
  Result Finish(grpc::Status* status, const TS& time_spec = time_point::max()) {
    return Finish(status, ToDeadline(time_spec));
  }

  template <typename TS = time_point>
  grpc::Status Finish(const TS& time_spec = time_point::max()) {
    return Finish(ToDeadline(time_spec));
  }

  grpc::Status FinishNow() {
    return Finish(time_point::min());
  };
};

template <typename W>
class IDuplexClientWriter : public virtual IDuplexClient {
 public:
  virtual ~IDuplexClientWriter() {}

  virtual Result Write(const W& obj, const gpr_timespec& deadline) = 0;
  virtual Result WriteAsync(const W& obj) = 0;

  // Templated methods

  template <typename TS = time_point>
  Result Write(const W& obj,
               const TS& time_spec = time_point::max()) {
    return Write(obj, ToDeadline(time_spec));
  }
};

// Base class for duplex clients.
class DuplexClient : public virtual IDuplexClient {
 public:
  // Status flags. These are the only valid bits that may be checked for in a call to `Poll`.
  enum FlagValues : Flags {
    STARTED = Done(Op::START),
    CAN_READ = Done(Op::READ),
    CAN_WRITE = Done(Op::WRITE),
    WRITES_DONE = Done(Op::WRITES_DONE),
    FINISHED = Done(Op::FINISH),
    SHUTDOWN = Done(Op::SHUTDOWN),

    STREAM_ERROR = static_cast<Flags>(1) << (sizeof(Flags) * 8 - 1),
  };

  // Make these accessible to the user in an easy manner, without having to export them at namespace level.
  using Status = grpc_duplex_impl::Status;
  using Result = grpc_duplex_impl::Result;

  virtual ~DuplexClient() = default;

  // Disallow moves and copies.

  DuplexClient(const DuplexClient&) = delete;
  DuplexClient(DuplexClient&&) = delete;

  DuplexClient& operator=(const DuplexClient&) = delete;
  DuplexClient& operator=(DuplexClient&&) = delete;

  // Wait for the specified time for the stream to become ready.
  Result WaitUntilStarted(const gpr_timespec& deadline) {
    return PollAll(STARTED, deadline);
  }

  // PollAny waits until the given time, or until an error in the stream occurs. A return value of true indicates that no
  // error occurred during the given time.
  bool Sleep(const gpr_timespec& deadline) {
    auto res = PollAny(STREAM_ERROR | FINISHED, deadline);
    return res.IsTimeout();
  }

  // WritesDone methods.

  Result WritesDoneAsync() {
    return Result(WritesDoneAsyncInternal().op_error);
  }

  Result WritesDone(const gpr_timespec& deadline) {
    return DoSync(&DuplexClient::WritesDoneAsyncInternal, deadline);
  }

  // Finish methods.

  Result FinishAsync() {
    return Result(FinishAsyncInternal().op_error);
  }

  Result WaitUntilFinished(const gpr_timespec& deadline) {
    return PollAll(FINISHED, deadline);
  }

  Result Finish(grpc::Status* status, const gpr_timespec& deadline) {
    auto res = DoSync<>(&DuplexClient::FinishAsyncInternal, deadline);
    if (res) {
      *status = status_;
    }
    return res;
  }

  grpc::Status Finish(const gpr_timespec& deadline) {
    auto res = DoSync<>(&DuplexClient::FinishAsyncInternal, deadline);
    if (res) {
      return status_;
    }
    if (res.IsTimeout()) {
      return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "timed out waiting for operation status");
    }
    return grpc::Status(grpc::StatusCode::UNKNOWN, "unknown error retrieving status");
  }

  // Poll waits until the given time for the status flag to match flags_checker.
  template <typename FlagsChecker>
  Result Poll(FlagsChecker&& flags_checker, const gpr_timespec& deadline) {
    Flags flags;

    Result res = ProcessSingle(&flags, deadline, nullptr);
    while (res && !flags_checker(flags)) {
      res = ProcessSingle(&flags, deadline, nullptr);
    }
    return res;
  }

  // PollAny waits until the given time for *any* of the specified status flags to be satisfied.
  Result PollAny(Flags desired, const gpr_timespec& deadline) {
    return Poll([desired](Flags fl) { return (fl & desired) != 0; }, deadline);
  }

  // PollAll waits until the given time for *all* specified status flags to be satisfied.
  Result PollAll(Flags desired, const gpr_timespec& deadline) {
    return Poll([desired](Flags fl) { return (fl & desired) == desired; }, deadline);
  }

  // Try cancelling the underlying context.
  void TryCancel() {
    context_->TryCancel();
  }

  // Shutdown the client.
  Result Shutdown() {
    context_->TryCancel();
    if (CheckFlags(Done(Op::SHUTDOWN))) {
      return Result(Status::ALREADY_DONE);
    }
    if (!SetFlags(Pending(Op::SHUTDOWN))) {
      return Result(Status::ALREADY_PENDING);
    }
    cq_.Shutdown();
    return Result(Status::OK);
  }

  // Static creation methods.

  template <typename Stub, typename W, typename R>
  static std::unique_ptr<DuplexClientReaderWriter<W, R>> Create(
      std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> (Stub::*create_method)(
          grpc::ClientContext* context,
          grpc::CompletionQueue* cq,
          void* tag),
      const std::shared_ptr<grpc::Channel>& channel,
      grpc::ClientContext* context) {
    return std::unique_ptr<DuplexClientReaderWriter<W, R>>(
        new DuplexClientReaderWriter<W, R>(create_method, channel, context, nullptr));
  }

  template <typename Stub, typename W, typename R>
  static std::unique_ptr<DuplexClientWriter<W>> CreateWithReadCallback(
      std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> (Stub::*create_method)(
          grpc::ClientContext* context,
          grpc::CompletionQueue* cq,
          void* tag),
      const std::shared_ptr<grpc::Channel>& channel,
      grpc::ClientContext* context,
      std::function<void(const R*)> read_callback) {
    return std::unique_ptr<DuplexClientWriter<W>>(
        new DuplexClientReaderWriter<W, R>(create_method, channel, context, std::move(read_callback)));
  }

  template <typename Stub, typename W, typename R>
  static std::unique_ptr<DuplexClientWriter<W>> CreateWithReadsIgnored(
      std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> (Stub::*create_method)(
          grpc::ClientContext* context,
          grpc::CompletionQueue* cq,
          void* tag),
      const std::shared_ptr<grpc::Channel>& channel,
      grpc::ClientContext* context) {
    std::function<void(const R*)> read_callback = [](const R*) {};
    return CreateWithReadCallback(create_method, channel, context, std::move(read_callback));
  }

 protected:
  DuplexClient(grpc::ClientContext* context) : context_(context) {}

  // SetFlags sets the given flags, and returns the newly set flags.
  Flags SetFlags(Flags fl) {
    Flags new_flags = fl & ~flags_;
    flags_ |= fl;
    return new_flags;
  }

  // ClearFlags clears the given flags, and returns the flags that were newly cleared.
  Flags ClearFlags(Flags fl) {
    Flags cleared_flags = fl & flags_;
    flags_ &= ~fl;
    return cleared_flags;
  }

  // CheckFlags checks which of the given flags are set.
  Flags CheckFlags(Flags fl) {
    return flags_ & fl;
  }

  // Perform the asynchronous operation specified by async_method in a synchronous manner.
  template <typename... Args, typename D>
  Result DoSync(OpDescriptor (D::*async_method)(Args...), Args... args, const gpr_timespec& deadline) {
    constexpr bool idempotent = is_empty<Args...>::value;

    OpDescriptor op_desc = (static_cast<D*>(this)->*async_method)(std::forward<Args>(args)...);
    // If an operation is already pending, we act as if it had just been sent in case the operation is idempotent (i.e.,
    // does not depend on a parameter).
    if (idempotent && op_desc.op_error == OpError::ALREADY_PENDING) {
      op_desc.op_error = OpError::OK;
    }
    if (op_desc.op_error != OpError::OK) {
      return Result(op_desc.op_error);
    }

    OpResult op_res;
    auto result = ProcessSingle(nullptr, deadline, &op_res);
    while (result && op_res.op != op_desc.op) {
      result = ProcessSingle(nullptr, deadline, &op_res);
    }
    if (!result) return result;
    return Result(op_res.ok);
  }

  virtual Result ProcessSingle(Flags* flags_out, const gpr_timespec& deadline, OpResult* op_res_out) = 0;
  virtual OpDescriptor WritesDoneAsyncInternal() = 0;
  virtual OpDescriptor FinishAsyncInternal() = 0;

  Flags flags_ = 0;
  grpc::Status status_;
  grpc::CompletionQueue cq_;
  grpc::ClientContext* context_;
};

template <typename W>
class DuplexClientWriter : public DuplexClient, public IDuplexClientWriter<W> {
 public:
  ~DuplexClientWriter() override = default;

  // Write methods.

  Result Write(const W& obj, const gpr_timespec& deadline) {
    return DoSync<const W&>(&DuplexClientWriter::WriteAsyncInternal, obj, deadline);
  }

  Result WriteAsync(const W& obj) {
    return Result(WriteAsyncInternal(obj));
  }

 protected:
  DuplexClientWriter(grpc::ClientContext* context) : DuplexClient(context) {}

  virtual OpDescriptor WriteAsyncInternal(const W& obj) = 0;
};

template <typename W, typename R>
class DuplexClientReaderWriter : public DuplexClientWriter<W> {
 public:
  ~DuplexClientReaderWriter() override {
    // Shutdown the client and drain the queue.
    this->Shutdown();  // ignore errors
    auto now = ToDeadline(time_point::min());
    while (ProcessSingle(nullptr, now, nullptr))
      ;
  }

  template <typename TS = time_point>
  Result Read(R* obj, const TS& time_spec = time_point::max()) {
    if (read_callback_) {
      return Result(Status::ILLEGAL_STATE);
    }

    auto deadline = ToDeadline(time_spec);

    auto result = PollAll(DuplexClient::CAN_READ, deadline);
    if (!result) return result;

    // We can read, but the read buffer is not valid -> there has been an error. This is the last read.
    if (!read_buf_valid_) {
      return Result(Status::ERROR);
    }

    if (obj) *obj = std::move(read_buf_);
    ReadNext();

    return Result(Status::OK);
  }

  Result TryRead(R* obj) {
    return Read(obj, time_point::min());
  }

 private:
  using RW = grpc::ClientAsyncReaderWriter<W, R>;

  template <typename Stub>
  DuplexClientReaderWriter(
      std::unique_ptr<RW> (Stub::*create_method)(grpc::ClientContext*, grpc::CompletionQueue*, void*),
      const std::shared_ptr<grpc::Channel>& channel,
      grpc::ClientContext* context,
      std::function<void(const R*)>&& read_callback)
      : DuplexClientWriter<W>(context), read_callback_(std::move(read_callback)) {
    Stub stub(channel);
    rw_ = (stub.*create_method)(context, &this->cq_, OpToTag(Op::START));
    this->SetFlags(Pending(Op::START));
    ReadNext();
  }

  // Perform the next read operation.
  void ReadNext() {
    read_buf_valid_ = false;
    OpDescriptor op_desc = ReadAsyncInternal();
    if (op_desc.op_error != OpError::OK) {
      return;
    }
  }

  // Perform an asynchronous operation.
  template <typename... Args>
  OpDescriptor DoAsync(void (RW::*async_func)(Args..., void*), Args... args, Op op) {
    constexpr bool idempotent = is_empty<Args...>::value;

    // For non-idempotent operations, we are ok with them being already done.
    if (idempotent && this->CheckFlags(Done(op))) {
      return {op, OpError::ALREADY_DONE};
    }
    if (this->CheckFlags(Pending(op))) {
      return {op, OpError::ALREADY_PENDING};
    }
    if (this->CheckFlags(Done(Op::SHUTDOWN) | Pending(Op::SHUTDOWN))) {
      return {op, OpError::SHUTDOWN};
    }

    // No more operations can be issued after finish has been requested (note that a duplicate async call to finish
    // is handled by the above).
    if (this->CheckFlags(Done(Op::FINISH) | Pending(Op::FINISH))) {
      return {op, OpError::ILLEGAL_STATE};
    }

    // Mark operation as pending.
    this->ClearFlags(Done(op));
    this->SetFlags(Pending(op));
    (rw_.get()->*async_func)(std::forward<Args>(args)..., OpToTag(op));
    return {op, OpError::OK};
  }

  // Async operation implementations. These wrap DoAsync around the corresponding GRPC AsyncClientReaderWriter methods.
  OpDescriptor WriteAsyncInternal(const W& obj) override {
    return DoAsync<const W&>(&RW::Write, obj, Op::WRITE);
  }

  OpDescriptor WritesDoneAsyncInternal() override {
    return DoAsync<>(&RW::WritesDone, Op::WRITES_DONE);
  }

  OpDescriptor FinishAsyncInternal() override {
    return DoAsync<grpc::Status*>(&RW::Finish, &this->status_, Op::FINISH);
  }

  OpDescriptor ReadAsyncInternal() {
    return DoAsync<R*>(&RW::Read, &read_buf_, Op::READ);
  }

  // Process a single event.
  Result ProcessSingle(Flags* flags_out, const gpr_timespec& deadline, OpResult* op_res_out) override {
    void* raw_tag;
    bool ok;

    auto next_status = this->cq_.AsyncNext(&raw_tag, &ok, deadline);
    if (next_status == grpc::CompletionQueue::GOT_EVENT) {
      Op op = TagToOp(raw_tag);
      ProcessEvent(op, ok);
      if (op_res_out) {
        op_res_out->op = op;
        op_res_out->ok = ok;
      }
    } else if (next_status == grpc::CompletionQueue::SHUTDOWN) {
      this->SetFlags(Done(Op::SHUTDOWN));
    }

    if (flags_out) *flags_out = this->flags_;
    return Result(next_status);
  }

  // Event handlers

  void HandleRead(bool ok) {
    if (this->read_callback_) {
      this->read_callback_(ok ? &read_buf_ : nullptr);
      ReadNext();
    } else {
      read_buf_valid_ = ok;
    }
  }

  void HandleFinish(bool ok) {
    if (!ok && this->status_.ok()) {
      this->status_ = grpc::Status(grpc::StatusCode::UNKNOWN, "Finish operation unsuccessful");
    }
  }

  void ProcessEvent(Op op, bool ok) {
    Flags fl = Done(op);
    // According to the completion queue doc, all failures on the client-side are permanent
    if (!ok) {
      FinishAsyncInternal();
      fl |= DuplexClient::STREAM_ERROR;
    }

    this->ClearFlags(Pending(op));
    this->SetFlags(fl);

    switch (op) {
      case Op::READ:
        HandleRead(ok);
        break;
      case Op::FINISH:
        HandleFinish(ok);
        break;
      default:
        break;
    }
  }

  std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> rw_;
  std::function<void(const R*)> read_callback_;
  R read_buf_;
  bool read_buf_valid_ = false;

  friend class DuplexClient;
};

}  // namespace grpc_duplex_impl

// Export public definitions.

using IDuplexClient = grpc_duplex_impl::IDuplexClient;
template <typename W>
using IDuplexClientWriter = grpc_duplex_impl::IDuplexClientWriter<W>;
using DuplexClient = grpc_duplex_impl::DuplexClient;
template <typename W>
using DuplexClientWriter = grpc_duplex_impl::DuplexClientWriter<W>;
template <typename W, typename R>
using DuplexClientReaderWriter = grpc_duplex_impl::DuplexClientReaderWriter<W, R>;

}  // namespace collector

#endif  // COLLECTOR_DUPLEXGRPC_H
