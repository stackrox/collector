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

// KafkaClient.cpp
// The KafkaClient implementation file

#include "KafkaClient.h"

#include <exception>
#include <string>
#include <iostream>

#include "Logging.h"

namespace collector {

// construction
KafkaClient::KafkaClient(const rd_kafka_conf_t* conf_template) {
  char errstr[1024];

  rd_kafka_conf_t* conf = rd_kafka_conf_dup(conf_template);
  rd_kafka_conf_res_t res = rd_kafka_conf_set(conf, "batch.num.messages", "10000", errstr, sizeof(errstr));
  if (res != RD_KAFKA_CONF_OK) {
    throw KafkaException(std::string("Error setting up Kafka config: ") + errstr);
  }

  res = rd_kafka_conf_set(conf, "queue.buffering.max.ms", "1000", errstr, sizeof(errstr));
  if (res != RD_KAFKA_CONF_OK) {
    throw KafkaException(std::string("Error setting up Kafka config: ") + errstr);
  }
  // api.version.request should be set to true on brokers >= v0.10.0.
  // See: https://github.com/edenhill/librdkafka/wiki/Broker-version-compatibility
  res = rd_kafka_conf_set(conf, "api.version.request", "true", errstr, sizeof(errstr));
  if (res != RD_KAFKA_CONF_OK) {
    throw KafkaException(std::string("Error setting up Kafka config: ") + errstr);
  }

  kafka_ = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
  if (!kafka_) {
    throw KafkaException(std::string("Error creating Kafka producer: ") + errstr);
  }
}

KafkaClient::~KafkaClient() {
  rd_kafka_destroy(kafka_);
}

rd_kafka_topic_t* KafkaClient::createTopic(const char* topic) {
  rd_kafka_topic_conf_t* topic_conf = rd_kafka_topic_conf_new();
  // We'll use a consistent-hash partitioner based on "key" fields. This allows us to
  // leverage the per-partition ordering guarantees of Kafka while producing and consuming
  // in parallel.
  // The key field values are provided to the producer when a message is ready to send.
  // See http://goo.gl/5CjisN for the semantics of this partitioner.
  rd_kafka_topic_conf_set_partitioner_cb(topic_conf, rd_kafka_msg_partitioner_consistent);
  rd_kafka_topic_t* kafkaTopic = rd_kafka_topic_new(kafka_, topic, topic_conf);

  return kafkaTopic;
}

bool KafkaClient::sendMessage(rd_kafka_topic_t* kafkaTopic, const void* msg, int msgLen, const void* key, int keyLen) {
  // rd_kafka_produce doesn't modify the payload if RD_KAFKA_MSG_F_COPY is specified, so the const_cast is safe.
  int rv = rd_kafka_produce(kafkaTopic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
       const_cast<void*>(msg), msgLen, key, keyLen, nullptr);

  if (rv == -1) {
    CLOG_THROTTLED(ERROR, std::chrono::seconds(5))
        << "Failed to produce to topic " << rd_kafka_topic_name(kafkaTopic) << ": "
        << rd_kafka_err2str(rd_kafka_last_error());
  }

  if (++send_count_ % 10000 == 0) {
    rd_kafka_poll(kafka_, 0);
  }

  return rv != -1;
}

KafkaClient::TopicHandle::TopicHandle(const std::string& topic_name, std::shared_ptr<KafkaClient> client)
    : client_(std::move(client)), topic_(client_->createTopic(topic_name.c_str())) {
}

KafkaClient::TopicHandle::~TopicHandle() {
  rd_kafka_topic_destroy(topic_);
}

bool KafkaClient::TopicHandle::Send(const void* msg, int msg_len, const void* key, int key_len) {
  return client_->sendMessage(topic_, msg, msg_len, key, key_len);
}

}  // namespace collector
