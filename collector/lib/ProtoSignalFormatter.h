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

#ifndef _PROTO_FORMATTER_H_
#define _PROTO_FORMATTER_H_

#include <utility>

#include <google/protobuf/message.h>

#ifdef USE_PROTO_ARENAS
#include <google/protobuf/arena.h>
#endif

#include "SignalFormatter.h"

namespace collector {

// Base class for all signal formatters that output protobuf messages.
class BaseProtoSignalFormatter : public SignalFormatter {
 public:
  BaseProtoSignalFormatter(bool text_format = false) : text_format_(text_format) {}

  bool FormatSignal(SafeBuffer* buf, sinsp_evt* event) override;

 protected:
  // Returns a pointer to the proto message derived from this event, or nullptr if no message should be output.
  // Note: The caller does not assume ownership of the returned message. To avoid an additional heap allocation, the
  // implementing class should maintain an instance-level message whose address is returned by this method.
  virtual void Reset() {}
  virtual const google::protobuf::Message* ToProtoMessage(sinsp_evt* event) = 0;

  template <typename T, typename... Args>
  T* Allocate(Args&&... args) { return new T(std::forward<Args>(args)...); }

 private:
  bool text_format_;
};

template <typename Message>
class ProtoSignalFormatter : public BaseProtoSignalFormatter {
 public:
  ProtoSignalFormatter(bool text_format = false)
      : BaseProtoSignalFormatter(text_format)
#ifdef USE_PROTO_ARENAS
      , arena_(ArenaOptionsForInitialBlock(arena_storage_, kArenaStorageSize))
#endif
  {}

#ifndef USE_PROTO_ARENAS
 protected:
  template <typename T, typename... Args>
  T* Allocate(Args&&... args) { return new T(std::forward<Args>(args)...); }

  Message* AllocateRoot() {
    message_.Clear();
    return &message_;
  }

 private:
  Message message_;

#else
 protected:
  void Reset() override {
    google::protobuf::uint64 bytes_used = arena_.Reset();
    if (bytes_used > kArenaStorageSize) {
      CLOG_THROTTLED(WARNING, std::chrono::seconds(5))
          << "Used " << bytes_used << " bytes in the arena, which is more than the pre-allocated "
          << kArenaStorageSize << "bytes. Consider increasing the pre-allocated size";
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
  static constexpr int kArenaStorageSize = 32768;

  static void* BlockAlloc(size_t size) {
    static void* (*default_block_alloc)(size_t) = google::protobuf::ArenaOptions().block_alloc;
    CLOG(WARNING) << "Allocating a memory block on the heap for the arena, this is inefficient and usually avoidable";
    return (*default_block_alloc)(size);
  }

  static google::protobuf::ArenaOptions ArenaOptionsForInitialBlock(char* storage, int size) {
    google::protobuf::ArenaOptions opts;
    opts.initial_block = storage;
    opts.initial_block_size = size;
    opts.block_alloc = &BlockAlloc;
    return opts;
  }

  google::protobuf::Arena arena_;
  char arena_storage_[kArenaStorageSize];
#endif
};

}  // namespace collector

#endif  // _PROTO_FORMATTER_H_
