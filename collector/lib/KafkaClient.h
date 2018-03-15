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

#ifndef __KAFKACLIENT_H
#define __KAFKACLIENT_H

// KafkaClient.h
// This class defines our Kafka abstraction

#include <memory>
#include <string>

extern "C" {
#include "librdkafka/rdkafka.h"
}

namespace collector {

class KafkaException : public std::exception {
 public:
  KafkaException(std::string message) : message_(std::move(message)) {}
  const char* what() const noexcept override { return message_.c_str(); }

 private:
  std::string message_;
};

class KafkaClient {
 public:
  KafkaClient(const std::string& brokerList);
  ~KafkaClient();

  class TopicHandle;

 private:
  // method to create a topic
  rd_kafka_topic_t* createTopic(const char* topic);

  // method to send to a specific topic
  bool sendMessage(rd_kafka_topic_t* kafkaTopic, const void* msg, int msgLen, const void* key, int keyLen);

  rd_kafka_t* kafka_;
  rd_kafka_topic_t* networkTopicHandle_ = nullptr;
  rd_kafka_topic_t* processTopicHandle_ = nullptr;
  rd_kafka_topic_t* fileTopicHandle_ = nullptr;

  uint64_t send_count_ = 0;
};

class KafkaClient::TopicHandle {
 public:
  TopicHandle(const std::string& topic, std::shared_ptr<KafkaClient> kafka);
  ~TopicHandle();

  bool Send(const void* msg, int msg_len, const void* key, int key_len);

 private:
  std::shared_ptr<KafkaClient> client_;
  rd_kafka_topic_t* topic_;
};

}  // namespace collector

#endif // __KAFKACLIENT_H
