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
#include "ProtoUtil.h"

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

  // stream writer
  context_ = MakeUnique<grpc::ClientContext>();
  context_->AddMetadata("rox-collector-hostname", hostname_);
  writer_ = DuplexClient::CreateWithReadsIgnored(&SignalService::Stub::AsyncPushSignals, channel_, context_.get());
  if (!writer_->WaitUntilStarted(std::chrono::seconds(30))) {
    CLOG(ERROR) << "Signal stream not ready after 30 seconds. Retrying ...";
    CLOG(ERROR) << "Error message: " << writer_->FinishNow().error_message();
    writer_.reset();
    return true;
  }
  CLOG(INFO) << "Successfully established GRPC stream for signals.";

  first_write_ = true;
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
  stream_interrupted_.notify_one();
  thread_.Stop();
  context_->TryCancel();
}

SignalHandler::Result SignalServiceClient::PushSignals(const SignalStreamMessage& msg) {
  if (!stream_active_.load(std::memory_order_acquire)) {
  	CLOG_THROTTLED(ERROR, std::chrono::seconds(10))
		  << "GRPC stream is not established";
    return SignalHandler::ERROR;
  }

  if (first_write_) {
    first_write_ = false;
    return SignalHandler::NEEDS_REFRESH;
  }

  if (!writer_->Write(msg)) {
    auto status = writer_->FinishNow();
    if (!status.ok()) {
      CLOG(ERROR) << "GRPC writes failed: " << status.error_message();
    }
    writer_.reset();

    stream_active_.store(false, std::memory_order_release);
    CLOG(ERROR) << "GRPC stream interrupted";
    stream_interrupted_.notify_one();
    return SignalHandler::ERROR;
  }

  return SignalHandler::PROCESSED;
}

} // namespace collector
