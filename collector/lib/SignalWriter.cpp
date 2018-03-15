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
#include "SignalWriter.h"

#include "CollectorException.h"
#include "KafkaSignalWriter.h"
#include "StdoutSignalWriter.h"

namespace collector {

void SignalWriterFactory::SetupKafka(const std::string& broker_list) {
  kafka_client_ = std::make_shared<KafkaClient>(broker_list);
}

std::unique_ptr<SignalWriter> SignalWriterFactory::CreateSignalWriter(const std::string& output_spec) {
  auto delim_pos = output_spec.find(':');
  std::string output_type = output_spec.substr(0, delim_pos);
  std::string spec;
  if (delim_pos != std::string::npos) {
    spec = output_spec.substr(delim_pos + 1);
  }

  if (output_type == "stdout") {
    return CreateStdoutSignalWriter(spec);
  }
  if (output_type == "kafka") {
    return CreateKafkaSignalWriter(spec);
  }
  throw CollectorException("Invalid output type '" + output_type + "' in output spec '" + output_spec + "'");
}

std::unique_ptr<SignalWriter> SignalWriterFactory::CreateStdoutSignalWriter(const std::string& spec) {
  return std::unique_ptr<SignalWriter>(new StdoutSignalWriter(spec));
}

std::unique_ptr<SignalWriter> SignalWriterFactory::CreateKafkaSignalWriter(const std::string& spec) {
  if (spec.empty()) {
    throw CollectorException("When using output type 'kafka', a non-empty topic must be supplied (i.e., "
                             "'kafka:<topic>')");
  }
  if (!kafka_client_) {
    throw CollectorException("Output type 'kafka' specified, but collector was not set up to use Kafka");
  }

  return std::unique_ptr<SignalWriter>(new KafkaSignalWriter(spec, kafka_client_));
}

}  // namespace collector