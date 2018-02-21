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

// construction
KafkaClient::KafkaClient(const std::string& brokerList, const std::string& networkTopic,
                         const std::string& processTopic, const std::string& fileTopic) {
  char errstr[1024];
  std::cout << "Building Kafka client with brokers: " << brokerList << std::endl;

  rd_kafka_conf_t* conf = rd_kafka_conf_new();
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

  int rv = rd_kafka_brokers_add(kafka_, brokerList.c_str());
  if (rv < 1) {
    throw KafkaException("No valid Kafka brokers specified: '" + brokerList + "'");
  }

  if (!networkTopic.empty()) {
    networkTopicHandle_ = createTopic(networkTopic.c_str());
  }
  if (!processTopic.empty()) {
    processTopicHandle_ = createTopic(processTopic.c_str());
  }
  if (!fileTopic.empty()) {
    fileTopicHandle_ = createTopic(fileTopic.c_str());
  }
}

KafkaClient::~KafkaClient() {
  if (networkTopicHandle_) {
    rd_kafka_topic_destroy(networkTopicHandle_);
  }
  if (processTopicHandle_) {
    rd_kafka_topic_destroy(processTopicHandle_);
  }
  if (fileTopicHandle_) {
    rd_kafka_topic_destroy(fileTopicHandle_);
  }

  if (kafka_) {
    rd_kafka_destroy(kafka_);
  }
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

bool KafkaClient::send(const void* msg, int msgLen, const void* key, int keyLen,
                       bool onNetworkTopic, bool onProcessTopic, bool onFileTopic) {
  bool result = true;
  if (onNetworkTopic && networkTopicHandle_) {
    result &= sendMessage(networkTopicHandle_, msg, msgLen, key, keyLen);
  }
  if (onProcessTopic && processTopicHandle_) {
    result &= sendMessage(processTopicHandle_, msg, msgLen, key, keyLen);
  }
  if (onFileTopic && fileTopicHandle_) {
    result &= sendMessage(fileTopicHandle_, msg, msgLen, key, keyLen);
  }
  if (++send_count_ % 10000 == 0) {
    rd_kafka_poll(kafka_, 0);
  }
  return result;
}

bool KafkaClient::sendMessage(rd_kafka_topic_t* kafkaTopic, const void* msg, int msgLen, const void* key, int keyLen) {
  // rd_kafka_produce doesn't modify the payload if RD_KAFKA_MSG_F_COPY is specified, so the const_cast is safe.
  int rv = rd_kafka_produce(kafkaTopic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
       const_cast<void*>(msg), msgLen, key, keyLen, nullptr);
  if (rv != -1) return true;
  std::cerr << "Failed to produce to topic " << rd_kafka_topic_name(kafkaTopic) << ": "
            << rd_kafka_err2str(rd_kafka_errno2err(errno)) << std::endl;
  return false;
}
