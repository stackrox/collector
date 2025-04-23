#pragma once

#ifdef USE_PROTO_ARENAS
#  include <google/protobuf/arena.h>
#endif

#include "Logging.h"

namespace collector {

namespace internal {

#ifdef USE_PROTO_ARENAS

inline google::protobuf::ArenaOptions ArenaOptionsForInitialBlock(char* storage, size_t size) {
  google::protobuf::ArenaOptions opts;
  opts.initial_block = storage;
  opts.initial_block_size = size;
  return opts;
}

template <typename Message>
class ArenaProtoAllocator {
 public:
  static constexpr size_t kDefaultPoolSize = 524288;

  ArenaProtoAllocator() : ArenaProtoAllocator(kDefaultPoolSize) {}

  explicit ArenaProtoAllocator(size_t pool_size)
      : pool_(new char[pool_size]), pool_size_(pool_size), arena_(ArenaOptionsForInitialBlock(pool_.get(), pool_size_)) {}

  void Reset() {
    google::protobuf::uint64 bytes_used = arena_.Reset();
    if (bytes_used > pool_size_) {
      size_t new_pool_size = (bytes_used / kDefaultPoolSize + 1) * kDefaultPoolSize;
      CLOG(WARNING) << "Used " << bytes_used << " bytes in the arena, which is more than the pre-allocated "
                    << pool_size_ << " bytes. Increasing arena size to " << new_pool_size << " bytes.";

      // This looks weird but is correct (search for `placement new/delete` on Google).
      arena_.~Arena();

      pool_.reset(new char[new_pool_size]);
      pool_size_ = new_pool_size;

      new (&arena_) google::protobuf::Arena(ArenaOptionsForInitialBlock(pool_.get(), pool_size_));
    }
  }

  template <typename T, typename... Args>
  T* Allocate(Args&&... args) {
    return google::protobuf::Arena::CreateMessage<T>(&arena_, std::forward<Args>(args)...);
  }

  Message* AllocateRoot() {
    return google::protobuf::Arena::CreateMessage<Message>(&arena_);
  }

 private:
  std::unique_ptr<char[]> pool_;
  size_t pool_size_;
  google::protobuf::Arena arena_;
};

#endif

template <typename Message>
class HeapProtoAllocator {
 public:
  HeapProtoAllocator() {}

  HeapProtoAllocator(size_t) : HeapProtoAllocator() {}

  void Reset() {}

  template <typename T, typename... Args>
  T* Allocate(Args&&... args) { return new T(std::forward<Args>(args)...); }

  Message* AllocateRoot() {
    message_.Clear();
    return &message_;
  }

 private:
  Message message_;
};

}  // namespace internal

template <typename Message>
using ProtoAllocator =
#ifndef USE_PROTO_ARENAS
    internal::HeapProtoAllocator<Message>;
#else
    internal::ArenaProtoAllocator<Message>;
#endif

}  // namespace collector
