
//
// Created by Malte Isberner on 9/22/18.
//

#ifndef COLLECTOR_DUPLEXGRPC_H
#define COLLECTOR_DUPLEXGRPC_H

#include <chrono>
#include <cstdint>

#include <grpcpp/grpcpp.h>

namespace collector {

namespace grpc_duplex_impl {

// Forward declarations

class DuplexClient;

template <typename W>
class DuplexClientWriter;

template <typename W, typename R>
class DuplexClientReaderWriter;

using clock = std::chrono::system_clock;
using time_point = clock::time_point;
using duration = clock::duration;


// Timespec -> deadline conversion functions.

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

using Flags = std::uint32_t;

enum class Op : std::uint8_t {
  START = 0,
  READ,
  WRITE,
  WRITES_DONE,
  FINISH,
  SHUTDOWN,

  MAX,
};



inline void* OpToTag(Op op) {
  auto ptr_val = static_cast<std::uintptr_t>(op);
  return reinterpret_cast<void*>(ptr_val);
}

inline Op TagToOp(void* tag) {
  auto ptr_val = reinterpret_cast<std::uintptr_t>(tag);
  return static_cast<Op>(ptr_val);
}

inline constexpr Flags Done(Op op) {
  return static_cast<Flags>(1) << static_cast<std::uint8_t>(op);
}

inline constexpr Flags Pending(Op op) {
  return static_cast<Flags>(1) << (static_cast<std::uint8_t>(op) + static_cast<uint8_t>(Op::MAX));
}

enum class OpError : std::uint8_t {
  OK = 0,
  ALREADY_PENDING,
  ALREADY_DONE,
  ILLEGAL_STATE,
  SHUTDOWN,
};

struct OpDescriptor {
  Op op;
  OpError op_error;
};

struct OpResult {
  Op op;
  bool ok;
};

template <typename... Args>
struct is_empty : std::false_type {};

template <>
struct is_empty<> : std::true_type {};

enum class Status {
  OK,
  ALREADY_PENDING,
  ALREADY_DONE,

  ERROR,
  TIMEOUT,           // timed out waiting for operation to finish
  INVALID_ARGUMENT,  // an invalid value was specified for the operation
  ILLEGAL_STATE,     // the operation is not valid in the current state
  SHUTDOWN,          // the client was shutdown

  INTERNAL_ERROR,
};

class Result final {
 public:
  explicit operator bool() const {
    return ok();
  }

  bool ok() const {
    return status_ == Status::OK;
  }

  bool done() const {
    return status_ == Status::OK || status_ == Status::ALREADY_DONE;
  }

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

  Status status_;

  friend class DuplexClient;
  template <typename W>
  friend class DuplexClientWriter;
  template <typename W, typename R>
  friend class DuplexClientReaderWriter;
};

class DuplexClient {
 public:
  enum FlagValues : Flags {
    STARTED = Done(Op::START),
    CAN_READ = Done(Op::READ),
    CAN_WRITE = Done(Op::WRITE),
    WRITES_DONE = Done(Op::WRITES_DONE),
    FINISHED = Done(Op::FINISH),
    SHUTDOWN = Done(Op::SHUTDOWN),

    STREAM_ERROR = static_cast<Flags>(1) << (sizeof(Flags) * 8 - 1),
  };

  // Wait for the specified time for the stream to become ready.
  template <typename TS = time_point>
  Result WaitUntilStarted(const TS& time_spec = time_point::max()) {
    return PollAll(STARTED, time_spec);
  }

  // Sleeps for the specified time, or until an error in the stream occurs. A return value of true indicates that no
  // error occurred during the given time.
  template <typename TS = time_point>
  bool Sleep(const TS& time_spec = time_point::max()) {
    auto res = PollAny(STREAM_ERROR | FINISHED);
    return res.IsTimeout();
  }

  // WritesDone methods.

  Result WritesDoneAsync() {
    return Result(WritesDoneAsyncInternal().op_error);
  }

  template <typename TS = time_point>
  Result WritesDone(const TS& time_spec = time_point::max()) {
    return DoSync(&DuplexClient::WritesDoneAsyncInternal, time_spec);
  }

  // Finish methods.

  Result FinishAsync() {
    return Result(FinishAsyncInternal().op_error);
  }

  template <typename TS = time_point>
  Result WaitUntilFinished(const TS& time_spec = time_point::max()) {
    return PollAll(FINISHED, time_spec);
  }

  template <typename TS = time_point>
  Result Finish(grpc::Status* status, const TS& time_spec = time_point::max()) {
    auto res = DoSync<>(&DuplexClient::FinishAsyncInternal, time_spec);
    if (res) {
      *status = status_;
    }
    return res;
  }

  template <typename TS = time_point>
  grpc::Status Finish(const TS& time_spec = time_point::max()) {
    auto res = DoSync<>(&DuplexClient::FinishAsyncInternal, time_spec);
    if (res) {
      return status_;
    }
    if (res.IsTimeout()) {
      return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "timed out waiting for operation status");
    }
    return grpc::Status(grpc::StatusCode::UNKNOWN, "unknown error retrieving status");
  }


  // Poll waits until the given time for the status flag to match flags_checker.
  template <typename FlagsChecker, typename TS = time_point>
  Result Poll(FlagsChecker&& flags_checker, const TS& time_spec = time_point::max()) {
    Flags flags;
    auto deadline = ToDeadline(time_spec);

    Result res = ProcessSingle(&flags, deadline, nullptr);
    while (res && !flags_checker(flags)) {
      res = ProcessSingle(&flags, deadline, nullptr);
    }
    return res;
  }

  // PollAny waits until the given time for *any* of the specified status flags to be satisfied.
  template <typename TS = time_point>
  Result PollAny(Flags desired, const TS& time_spec = time_point::max()) {
    return Poll([desired](Flags fl) { return (fl & desired) != 0; }, time_spec);
  }

  // PollAll waits until the given time for *all* specified status flags to be satisfied.
  template <typename TS = time_point>
  Result PollAll(Flags desired, const TS& time_spec = time_point::max()) {
    return Poll([desired](Flags fl) { return (fl & desired) == desired; }, time_spec);
  }

  // Static creation methods.

  template <typename Stub, typename W, typename R>
  static std::unique_ptr<DuplexClientReaderWriter<W, R>> Create(
      std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> (Stub::*create_method)(
          grpc::ClientContext* context,
          grpc::CompletionQueue* cq,
          void* tag),
      const std::shared_ptr<grpc::Channel>& channel,
      grpc::ClientContext* context,
      std::function<void(const R*)> read_callback = nullptr) {
    return std::unique_ptr<DuplexClientReaderWriter<W, R>>(
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
    return Create(create_method, channel, context, std::move(read_callback));
  }

 protected:
  // SetFlags sets the given flags, and returns the flags that were newly set.
  Flags SetFlags(Flags fl) {
    Flags new_flags = fl & ~flags_;
    flags_ |= fl;
    return new_flags;
  }

  Flags ClearFlags(Flags fl) {
    Flags cleared_flags = fl & ~flags_;
    flags_ &= ~fl;
    return cleared_flags;
  }

  // CheckFlags checks which of the given flags are set.
  Flags CheckFlags(Flags fl) {
    return flags_ & fl;
  }

  template <typename... Args, typename TS, typename D>
  Result DoSync(OpDescriptor (D::*async_method)(Args...), Args... args, const TS& time_spec) {
    auto deadline = ToDeadline(time_spec);

    OpDescriptor op_desc = (static_cast<D*>(this)->*async_method)(std::forward<Args>(args)...);
    // If an operation is already pending, we act as if it had just been sent in case the operation is idempotent (i.e.,
    // does not depend on a parameter).
    if (is_empty<Args...>::value && op_desc.op_error == OpError::ALREADY_PENDING) {
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
};

template <typename W>
class DuplexClientWriter : public DuplexClient {
 public:
  template <typename TS = time_point>
  Result Write(const W& obj, const TS& time_spec = time_point::max()) {
    return DoSync<const W&>(&DuplexClientWriter::WriteAsyncInternal, obj, time_spec);
  }

  Result WriteAsync(const W& obj) {
    return Result(WriteAsyncInternal(obj));
  }

 protected:
  virtual OpDescriptor WriteAsyncInternal(const W& obj) = 0;
};

template <typename W, typename R>
class DuplexClientReaderWriter : public DuplexClientWriter<W> {
 public:
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
      : read_callback_(std::move(read_callback)) {
    Stub stub(channel);
    rw_ = (stub.*create_method)(context, &cq_, OpToTag(Op::START));
    this->SetFlags(Pending(Op::START));
    ReadNext();
  }

  void ReadNext() {
    read_buf_valid_ = false;
    OpDescriptor op_desc = DoAsync<R*>(&RW::Read, &read_buf_, Op::READ);
    if (op_desc.op_error != OpError::OK) {
      return;
    }
  }

  // Perform an asynchronous operation.
  template <typename... Args>
  OpDescriptor DoAsync(void (RW::*async_func)(Args..., void*), Args... args, Op op) {
    constexpr bool idempotent = is_empty<Args...>::value;

    // For non-idempotent operations, we are ok with them being already done.
    if (!idempotent && this->CheckFlags(Done(op))) {
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

  Result ProcessSingle(Flags* flags_out, const gpr_timespec& deadline, OpResult* op_res_out) override {
    void* raw_tag;
    bool ok;

    auto next_status = cq_.AsyncNext(&raw_tag, &ok, deadline);
    if (next_status == grpc::CompletionQueue::GOT_EVENT) {
      Op op = TagToOp(raw_tag);
      ProcessEvent(op, ok);
      if (op_res_out) {
        op_res_out->op = op;
        op_res_out->ok = ok;
      }
    }
    if (flags_out) *flags_out = this->flags_;
    return Result(next_status);
  }

  // Event handlers

  void HandleRead(bool ok) {
    if (this->read_callback_) {
      this->read_callback_(ok ? &read_buf_ : nullptr);
      ReadNext();
    }
  }

  void HandleFinish(bool ok) {
    if (!ok && this->status_.ok()) {
      this->status_ = grpc::Status(grpc::StatusCode::UNKNOWN, "Finish operation unsuccessful");
    }
  }

  void ProcessEvent(Op op, bool ok) {
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

    Flags fl = Done(op);
    // According to the completion queue doc, all failures on the client-side are permanent
    if (!ok) {
      FinishAsyncInternal();
      fl |= DuplexClient::STREAM_ERROR;
    }

    this->ClearFlags(Pending(op));
    this->SetFlags(fl);
  }

  std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> rw_;
  grpc::CompletionQueue cq_;
  std::function<void(const R*)> read_callback_;
  R read_buf_;
  bool read_buf_valid_ = false;

  friend class DuplexClient;
};

}  // namespace grpc_duplex_impl

using DuplexClient = grpc_duplex_impl::DuplexClient;
template <typename W>
using DuplexClientWriter = grpc_duplex_impl::DuplexClientWriter<W>;
template <typename W, typename R>
using DuplexClientReaderWriter = grpc_duplex_impl::DuplexClientReaderWriter<W, R>;

using Result = grpc_duplex_impl::Result;

}  // namespace collector

#endif //COLLECTOR_DUPLEXGRPC_H
