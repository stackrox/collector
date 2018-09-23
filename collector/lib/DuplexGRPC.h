
//
// Created by Malte Isberner on 9/22/18.
//

#ifndef COLLECTOR_DUPLEXGRPC_H
#define COLLECTOR_DUPLEXGRPC_H

namespace collector {


class DuplexClientDefinitions {
 public:
  using clock = std::chrono::system_clock;
  using time_point = clock::time_point;
  using duration = clock::duration;

  using Tag = uintptr_t;
  using Flags = std::uint32_t;

  enum FlagValues : Flags {
    STARTED = 1U << 1,
    CAN_READ = 1U << 2,
    WRITES_DONE_REQ = 1U << 3,
    WRITES_DONE = 1U << 4,
    FINISH_REQ = 1U << 5,
    FINISHED = 1U << 6,
    HAS_WRITTEN = 1U << 7,
    STREAM_ERROR = 1U << 8,
  };

  class Result {
   public:
    explicit Result(grpc::CompletionQueue::NextStatus status, bool ok) {
      switch (status) {
      }
    }

    explicit Result(Tag tag) {
      if (tag == ALREADY_DONE_TAG) {
        status_ = ALREADY_DONE;
      } else if (tag == ERROR_TAG) {
        status_ = ERROR;
      } else if (tag == SHUTDOWN_TAG) {
        status_ = SHUTDOWN;
      } else if (tag >= FIRST_VALID_TAG) {
        status_ = OK;
      } else {
        status_ = ERROR;
      }
    }

    explicit operator bool() const {
      return ok();
    }

    bool ok() const {
      return status_ == OK;
    }

    bool IsTimeout() const {
      return status_ == TIMEOUT;
    }

    bool IsPermanentError() const {
      return status_ == ERROR || status_ == SHUTDOWN;
    }

    bool CanContinue() const {
      return status_ == OK || status_ == ERROR;
    }
   private:
    enum {
      OK,
      ALREADY_DONE,

      ERROR,
      TIMEOUT,
      SHUTDOWN,
    } status_;
  };

 protected:
  enum TagValues : Tag {
    ERROR_TAG = 0,
    ALREADY_DONE_TAG = 1,
    SHUTDOWN_TAG,

    START_TAG,
    READ_TAG,
    WRITES_DONE_TAG,
    FINISH_TAG,
    WRITE_TAG_BASE,
  };

  static constexpr Tag FIRST_VALID_TAG = START_TAG;

  static void* TagToPtr(Tag tag) {
    return reinterpret_cast<void*>(tag);
  }

  static Tag PtrToTag(void* ptr) {
    return reinterpret_cast<Tag>(ptr);
  }

  static constexpr Flags ALL_FLAGS = static_cast<Flags>(~0U);
  static constexpr Flags TRANSIENT_FLAGS = HAS_WRITTEN;
};

template <typename W>
class DuplexClientWriter;

template <typename W, typename R>
class DuplexClientReaderWriter;

class DuplexClient : public DuplexClientDefinitions {
 public:

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

  // WaitForWrites waits for the specified time until there are no more than max_outstanding writes outstanding.
  template <typename TS = time_point>
  Result WaitForWrites(int max_outstanding = 0, const TS& time_spec = time_point::max()) {
    auto deadline = ToDeadline(time_spec);
    while (outstanding_writes_ > max_outstanding) {
      auto res = ProcessSingle(nullptr, deadline, nullptr);
      if (!res) return res;
    }

    return Result();
  }

  // WritesDone methods.

  void WritesDoneAsync() {
    WritesDoneAsyncInternal();
  }

  template <typename TS = time_point>
  Result WaitUntilWritesDone(const TS& time_spec = time_point::max()) {
    return PollAll(WRITES_DONE, time_spec);
  }

  template <typename TS = time_point>
  Result WritesDone(const TS& time_spec = time_point::max()) {
    return DoSync(&DuplexClient::WritesDoneAsyncInternal, time_spec);
  }

  // Finish methods.

  void FinishAsync() {
    FinishAsyncInternal();
  }

  template <typename TS = time_point>
  Result WaitUntilFinished(const TS& time_spec = time_point::max()) {
    return PollAll(FINISHED, time_spec);
  }

  template <typename TS = time_point>
  Result Finish(grpc::Status* status, const TS& time_spec = time_point::max()) {
    auto res = AsSync(&DuplexClient::FinishAsyncInternal, time_spec);
    if (res) {
      *status = status_;
    }
    return res;
  }

  template <typename TS = time_point>
  grpc::Status Finish(const TS& time_spec = time_point::max()) {
    auto res = AsSync(&DuplexClient::FinishAsyncInternal, time_spec);
    if (res) {
      return status_;
    }
    return res.ToStatus();
  }

  template <typename TS = time_point>
  Result WaitForEvent(Tag expected_tag, bool* ok_out, const TS& time_spec = time_point::max()) {
    auto deadline = ToDeadline(time_spec);

    Tag tag;
    bool ok;
    auto res = ProcessSingle(nullptr, deadline, &tag, &ok);
    while (res && tag != expected_tag) {
      res = ProcessSingle(nullptr, deadline, &tag, &ok);
    }
    if (res && ok_out) *ok_out = ok;
    return res;
  }

  // Poll waits until the given time for the status flag to match flags_checker.
  template <typename FlagsChecker, typename TS = time_point>
  Result Poll(FlagsChecker&& flags_checker, const TS& time_spec = time_point::max()) {
    Flags flags;
    auto deadline = ToDeadline(time_spec);

    Result res = ProcessSingle(&flags, deadline, nullptr, nullptr);
    while (res.CanContinue() && !flags_checker(flags)) {
      flags = Flags(0);
      res = ProcessSingle(&flags, deadline, nullptr, nullptr);
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
    Stub stub(channel);

    std::unique_ptr<DuplexClientReaderWriter<W, R>> duplex_rw(new DuplexClientReaderWriter<W, R>(std::move(read_callback)));
    duplex_rw->Init((stub.*create_method)(context, &duplex_rw->cq_, START_TAG));

    return duplex_rw;
  }

  template <typename Stub, typename W, typename R>
  static std::unique_ptr<DuplexClientWriter<W>> CreateWithReadsIgnored(
      std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> (Stub::*create_method)(
          grpc::ClientContext* context,
          grpc::CompletionQueue* cq,
          void* tag),
      const std::shared_ptr<grpc::Channel>& channel,
      grpc::ClientContext* context) {
    return CreateWithReadCallback(create_method, channel, context, [] {});
  }

 protected:
  // Timespec -> deadline conversion functions.

  template <typename TS>
  static gpr_timespec ToDeadline(const TS& time_spec) {
    return grpc::TimePoint<TS>(time_spec).raw_time();
  }

  template <typename Rep, typename Period>
  static gpr_timespec ToDeadline(const std::chrono::duration<Rep, Period>& timeout) {
    return ToDeadline(clock::now() + std::chrono::duration_cast<duration>(timeout));
  }

  // SetFlags sets the given flags, and returns the flags that were newly set.
  Flags SetFlags(Flags fl) {
    Flags new_flags = fl & ~flags_;
    flags_ |= fl;
    return new_flags;
  }

  // CheckFlags checks which of the given flags are set.
  Flags CheckFlags(Flags fl) {
    return flags_ & fl;
  }

  template <typename TS, typename D, typename... Args>
  Result AsSync(Tag (D::*async_method)(Args...), Args... args, const TS& time_spec, Tag* tag_out) {
    auto deadline = ToDeadline(time_spec);

    Tag tag;
    if ((tag = (static_cast<D*>(this)->*async_method)(args..., &tag)) < FIRST_VALID_TAG) {
      return Result(tag);
    }

    auto result = WaitForEventInternal(tag, deadline);
    if (result.IsTimeout()) {
      *tag_out = tag;
    }
    return result;
  }

  virtual Result ProcessSingle(Flags* flags, gpr_timespec deadline, Tag* tag_out) = 0;
  virtual Tag WritesDoneAsyncInternal() = 0;
  virtual Tag FinishAsyncInternal() = 0;

  Flags flags_;
  grpc::Status status_;
  int outstanding_writes_;
};

template <typename W>
class DuplexClientWriter : public DuplexClient, private DuplexClientDefinitions {
 public:
  virtual bool WriteAsyncInternal(const W& obj, Tag* tag_out) = 0;

  template <typename TS = time_point>
  Result Write(const W& obj, const TS& time_spec = time_point::max(), Tag* tag_out = nullptr) {
    return AsSync(&DuplexClientWriter::WriteAsyncInternal, obj, time_spec, tag_out);
  }

  Tag WriteAsync(const W& obj) {
    return WriteAsyncInternal(obj);
  }

 protected:
  virtual Tag WriteAsyncInternal(const W& obj) = 0;
};

template <typename W, typename R>
class DuplexClientReaderWriter : public DuplexClientWriter<W>, private DuplexClientDefinitions {
 public:
  template <typename TS = time_point>
  Result Read(R* obj, const TS& time_spec = time_point::max()) {
    auto deadline = ToDeadline(time_spec);

    auto result = PollUntil(CAN_READ | READS_DONE, deadline);
    if (!result) return result;

    if (CheckFlags(CAN_READ)) {
      *obj = std::move(read_buf_);
      rw_->Read(&read_buf_, READ_TAG);
      ClearFlags(CAN_READ);
    } else {

    }
  }

 protected:
  DuplexClientReaderWriter(std::function<void(R*)>&& read_callback) : read_callback_(std::move(read_callback)) {}

  bool Init(std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> rw) {
    if (rw_) return false;

    rw_ = std::move(rw);
    rw_->Read(READ_TAG);
    return true;
  }

  Tag WriteAsyncInternal(const W& obj) override {
    if (CheckFlags(WRITES_DONE_REQ)) {
      return INVALID_TAG;
    }
    Tag tag = NextWriteTag();
    ++outstanding_writes_;
    rw_->Write(obj, TagToPtr(tag));
  }

  Tag WritesDoneAsyncInternal() override {
    if (SetFlags(WRITES_DONE_REQ) != WRITES_DONE_REQ) {
      return ALREADY_DONE_TAG;
    }
    rw_->WritesDone(TagToPtr(WRITES_DONE_TAG));
    return WRITES_DONE_TAG;
  }

  Tag FinishAsyncInternal() override {
    if (SetFlags(FINISH_REQ) != FINISH_REQ) {
      return ALREADY_DONE_TAG;
    }
    rw_->Finish(&status_, TagToPtr(FINISH_TAG));
    return FINISH_TAG;
  }

  Result ProcessSingle(Flags* flags, gpr_timespec deadline, Tag* tag_out) override {
    Tag tag;
    bool ok;
    auto next_status = cq_.AsyncNext(&tag, &ok, deadline);
    if (next_status == grpc::CompletionQueue::GOT_EVENT) {
      ProcessEvent(tag, ok);
      if (tag_out) *tag_out = tag;
    }
    return Result(next_status, ok);
  }

  Flags HandleStart(bool ok) {
    if (ok) return STARTED;
    return Flags(0);
  }

  Flags HandleRead(bool ok) {
    if (read_handler_) {
      read_handler_(ok ? &read_buf_ : nullptr);
    } else if (ok) {
      return CAN_READ;
    }
    return Flags(0);
  }

  Flags HandleWrite(bool ok) {
    --outstanding_writes_;
    if (ok) return HAS_WRITTEN;
    return Flags(0);
  }

  Flags HandleWritesDone(bool ok) {
    if (ok) return WRITES_DONE;
    return Flags(0);
  }

  Flags HandleFinish(bool ok) {
    if (ok) return FINISHED;
    status_ = grpc::Status(grpc::StatusCode::UNKNOWN, "Finish operation unsuccessful");
    return Flags(0);
  }

  void ProcessEvent(Tag tag, bool ok) {
    Flags fl;
    if (tag == START_TAG) {
      fl = HandleStart(ok);
    } else if (tag == READ_TAG) {
      fl = HandleRead(ok);
    } else if (tag == WRITES_DONE_TAG) {
      fl = HandleWritesDone(ok);
    } else if (tag == FINISH_TAG) {
      fl = HandleFinish(ok);
    } else if (tag >= WRITE_TAG_BASE) {
      fl = HandleWrite(ok);
    } else {
      // ignore, but shouldn't happen.
    }

    // According to the completion queue doc, all failures on the client-side are permanent
    if (!ok) {
      FinishAsyncInternal();
      fl |= STREAM_DEAD;
    }
    SetFlags(fl);
  }

  std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> rw_;
  grpc::CompletionQueue cq_;
  grpc::Status status_;
  std::function<void(const R*)> read_callback_;
  int outstanding_writes_;

  R read_buf_;
};

template <typename Stub, typename W, typename R>
std::unique_ptr<DuplexClientReaderWriter<W, R>> CreateDuplexClientReaderWriter(
    const std::shared_ptr<grpc::Channel>& channel,
    grpc::ClientContext* client_context,
    std::unique_ptr<grpc::ClientAsyncReaderWriter<W, R>> (Stub::*method)(grpc::ClientContext*, grpc::CompletionQueue*, void*)) {
  Stub stub(channel);
  std::unique_ptr<grpc::CompletionQueue> cq(new grpc::CompletionQueue);

  auto reader_writer = (stub.*method)(client_context, cq.get(), (void*)0x1);
  return {new DuplexClientReaderWriter<W, R>(std::move(reader_writer), std::move(cq))};
}


}  // namespace collector

#endif //COLLECTOR_DUPLEXGRPC_H
