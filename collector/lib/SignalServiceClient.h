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

#ifndef __SIGNAL_SERVICE_CLIENT_H
#define __SIGNAL_SERVICE_CLIENT_H

// SIGNAL_SERVICE_CLIENT.h
// This class defines our GRPC client abstraction

#include <mutex>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/signal_iservice.grpc.pb.h"

#include "CollectorService.h"
#include "DuplexGRPC.h"
#include "SignalHandler.h"
#include "StoppableThread.h"

namespace collector {

class SignalServiceClient {
 public:
  using SignalService = sensor::SignalService;
  using SignalStreamMessage = sensor::SignalStreamMessage;

  explicit SignalServiceClient(std::shared_ptr<grpc::Channel> channel)
      : channel_(std::move(channel)), stream_active_(false) {}

  void Start();
  void Stop();

  SignalHandler::Result PushSignals(const SignalStreamMessage& msg);

 private:
  void EstablishGRPCStream();
  bool EstablishGRPCStreamSingle();

  std::shared_ptr<grpc::Channel> channel_;

  StoppableThread thread_;
  std::atomic<bool> stream_active_;
  std::condition_variable stream_interrupted_;

  // This needs to have the same lifetime as the class.
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<SignalStreamMessage>> writer_;

  bool first_write_;
};

}  // namespace collector

#endif  // __SIGNAL_SERVICE_CLIENT_H
