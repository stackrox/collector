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

#include "SignalServiceClient.h"
#include "GRPCUtil.h"
#include "Logging.h"
#include "Utility.h"

#include <fstream>

namespace collector {

bool SignalServiceClient::EstablishGRPCStreamSingle() {
  std::mutex mtx;
  std::unique_lock<std::mutex> lock(mtx);
  stream_interrupted_.wait(lock, [this]() { return !stream_active_.load(std::memory_order_acquire) || thread_.should_stop(); });
  if (thread_.should_stop()) {
    return false;
  }

  CLOG(INFO) << "Trying to establish GRPC stream for signals ...";

  if (!WaitForChannelReady(channel_, [this]() { return thread_.should_stop(); })) {
    return false;
  }
  if (thread_.should_stop()) {
    return false;
  }

  CLOG(INFO) << "Successfully established GRPC stream for signals.";

  // stream writer
  v1::Empty empty;
  grpc_writer_ = stub_->PushSignals(&context_, &empty);

  stream_active_.store(true, std::memory_order_release);
  return true;
}

void SignalServiceClient::EstablishGRPCStream() {
  while (EstablishGRPCStreamSingle());
  CLOG(INFO) << "Signal service client terminating.";
}

void SignalServiceClient::Start() {
  thread_.Start([this]{EstablishGRPCStream();});
}

void SignalServiceClient::Stop() {
  thread_.Stop();
  context_.TryCancel();
  stream_interrupted_.notify_one();
}

bool SignalServiceClient::PushSignals(const SafeBuffer& msg) {
  if (!stream_active_.load(std::memory_order_acquire)) {
  	CLOG_THROTTLED(ERROR, std::chrono::seconds(10))
		  << "GRPC stream is not established";
    return false;
  }

  if (!GRPCWriteRaw(grpc_writer_.get(), msg)) {
    auto status = grpc_writer_->Finish();
    if (!status.ok()) {
      CLOG(ERROR) << "GRPC writes failed: " << status.error_message();
    }
    grpc_writer_.reset();

    stream_active_.store(false, std::memory_order_release);
    CLOG(ERROR) << "GRPC stream interrupted";
    stream_interrupted_.notify_one();
    return false;
  }

  return true;
}

} // namespace collector
