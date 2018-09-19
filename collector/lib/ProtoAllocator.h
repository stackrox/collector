//
// Created by Malte Isberner on 9/18/18.
//

#ifndef COLLECTOR_PROTOALLOCATOR_H
#define COLLECTOR_PROTOALLOCATOR_H

#ifdef USE_PROTO_ARENAS
#include <google/protobuf/arena.h>
#endif

#include "Logging.h"

namespace collector {

namespace internal {

#ifdef USE_PROTO_ARENAS

inline void* BlockAlloc(size_t size) {
  static void* (* default_block_alloc)(size_t) = google::protobuf::ArenaOptions().block_alloc;
  CLOG(WARNING) << "Allocating a memory block on the heap for the arena, this is inefficient and usually avoidable";
  return (*default_block_alloc)(size);
}

inline google::protobuf::ArenaOptions ArenaOptionsForInitialBlock(char* storage, size_t size) {
  google::protobuf::ArenaOptions opts;
  opts.initial_block = storage;
  opts.initial_block_size = size;
  opts.block_alloc = &BlockAlloc;
  return opts;
}

template <typename Message>
class ArenaProtoAllocator {
 public:
  static constexpr size_t kDefaultPoolSize = 32768;

  ArenaProtoAllocator() : ArenaProtoAllocator(kDefaultPoolSize) {}

  explicit ArenaProtoAllocator(size_t pool_size)
      : pool_(new char[pool_size]), pool_size_(pool_size),
        arena_(ArenaOptionsForInitialBlock(pool_.get(), pool_size_)) {}

  void Reset() {
    google::protobuf::uint64 bytes_used = arena_.Reset();
    if (bytes_used > pool_size_) {
      CLOG_THROTTLED(WARNING, std::chrono::seconds(5))
            << "Used " << bytes_used << " bytes in the arena, which is more than the pre-allocated "
            << pool_size_ << "bytes. Consider increasing the pre-allocated size";
    }
  }

  template <typename T, typename... Args>
  T* Allocate(Args&& ... args) {
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
  T* Allocate(Args&& ... args) { return new T(std::forward<Args>(args)...); }

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

#endif //COLLECTOR_PROTOALLOCATOR_H
