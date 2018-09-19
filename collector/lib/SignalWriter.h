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

#ifndef _SIGNAL_WRITER_H_
#define _SIGNAL_WRITER_H_

#include <memory>

#include <grpcpp/channel.h>

#include "librdkafka/rdkafka.h"

#include "EventClassifier.h"
#include "KafkaClient.h"
#include "SafeBuffer.h"
#include "SignalServiceClient.h"

namespace collector {

class SignalWriter {
 public:
  virtual bool WriteSignal(const SafeBuffer& msg, const SafeBuffer& key) = 0;
  virtual ~SignalWriter() = default;
};

class SignalWriterFactory {
 public:
  std::unique_ptr<SignalWriter> CreateSignalWriter(const std::string& output_spec);

  void SetupKafka(const rd_kafka_conf_t* conf_template);
  void SetupGRPC(std::shared_ptr<grpc::Channel> config);

 private:
  std::unique_ptr<SignalWriter> CreateStdoutSignalWriter(const std::string& spec);
  std::unique_ptr<SignalWriter> CreateKafkaSignalWriter(const std::string& spec);
  std::unique_ptr<SignalWriter> CreateGRPCSignalWriter(const std::string& spec);

  std::shared_ptr<KafkaClient> kafka_client_;
  std::shared_ptr<SignalServiceClient> grpc_client_;
};

}  // namespace collector

#endif  // _SIGNAL_WRITER_H_
