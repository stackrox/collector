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

#include <algorithm>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>

extern "C" {
  #include <unistd.h>
}

#include "sinsp.h"
#include "eventformatter.h"
#include "KafkaClient.h"

// construction
KafkaClient::KafkaClient(std::string brokerList, std::string _defaultTopic, std::string _networkTopic,
                         std::string _processTopic, std::string _fileTopic) {
  char errstr[1024];
  brokers = brokerList;
  defaultTopic = _defaultTopic;
  networkTopic = _networkTopic;
  processTopic = _processTopic;
  fileTopic = _fileTopic;
  kafka = NULL;
  defaultTopicHandle = NULL;
  networkTopicHandle = NULL;
  processTopicHandle = NULL;
  fileTopicHandle = NULL;

  // Get the hostname of this container.
  const string file = "/etc/hostname";
  std::ifstream f(file);
  if (!f) {
      std::cerr << "Unable to get hostname from " << file << std::endl;
      return;
  }
  std::string hostname;
  f >> hostname;
  f.close();

  containerID = new char[SHORT_CONTAINER_ID_LENGTH + 1];
  strncpy(containerID, hostname.c_str(), SHORT_CONTAINER_ID_LENGTH + 1);
  containerID[SHORT_CONTAINER_ID_LENGTH] = '\0';
  cout << "Container ID: " << containerID << endl;

  cout << "Building Kafka client with brokers: " << brokers << endl;

  conf = rd_kafka_conf_new();
  res = rd_kafka_conf_set(conf, "batch.num.messages", "1000",
        errstr, sizeof(errstr));
  if (res != RD_KAFKA_CONF_OK) {
    fprintf(stderr, "%s\n", errstr);
    return;
  }
  res = rd_kafka_conf_set(conf, "queue.buffering.max.ms", "1000",
        errstr, sizeof(errstr));
  if (res != RD_KAFKA_CONF_OK) {
    fprintf(stderr, "%s\n", errstr);
    return;
  }
  // api.version.request should be set to true on brokers >= v0.10.0.
  // See: https://github.com/edenhill/librdkafka/wiki/Broker-version-compatibility
  res = rd_kafka_conf_set(conf, "api.version.request", "true",
        errstr, sizeof(errstr));
  if (res != RD_KAFKA_CONF_OK) {
    fprintf(stderr, "%s\n", errstr);
    return;
  }

  kafka = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
  if (kafka == NULL) {
    fprintf(stderr, "%s\n", errstr);
    return;
  }

  int rv = rd_kafka_brokers_add(kafka, (char*)brokers.c_str());
  if (rv < 1) {
    fprintf(stderr, "No valid brokers specified\n");
    kafka = NULL;
    return;
  }

  if (!defaultTopic.empty()) {
    defaultTopicHandle = createTopic(defaultTopic.c_str());
  }
  if (!networkTopic.empty()) {
    networkTopicHandle = createTopic(networkTopic.c_str());
  }
  if (!processTopic.empty()) {
    processTopicHandle = createTopic(processTopic.c_str());
  }
  if (!fileTopic.empty()) {
    fileTopicHandle = createTopic(fileTopic.c_str());
  }
}

KafkaClient::~KafkaClient() {
  if (defaultTopicHandle != NULL) {
    rd_kafka_topic_destroy(defaultTopicHandle);
  }
  if (networkTopicHandle != NULL) {
    rd_kafka_topic_destroy(networkTopicHandle);
  }
  if (processTopicHandle != NULL) {
    rd_kafka_topic_destroy(processTopicHandle);
  }
  if (fileTopicHandle) {
    rd_kafka_topic_destroy(fileTopicHandle);
  }

  if (kafka != NULL)
    rd_kafka_destroy(kafka);
}

rd_kafka_topic_t* KafkaClient::createTopic(const char* topic) {
  rd_kafka_topic_conf_t* topic_conf;
  rd_kafka_topic_t* kafkaTopic;

  topic_conf = rd_kafka_topic_conf_new();
  // We'll use a consistent-hash partitioner based on "key" fields. This allows us to
  // leverage the per-partition ordering guarantees of Kafka while producing and consuming
  // in parallel.
  // The key field values are provided to the producer when a message is ready to send.
  // See http://goo.gl/5CjisN for the semantics of this partitioner.
  rd_kafka_topic_conf_set_partitioner_cb(topic_conf, rd_kafka_msg_partitioner_consistent);
  kafkaTopic = rd_kafka_topic_new(kafka, topic, topic_conf);
  topic_conf = NULL;

  cout << "Created topic object: " << topic << endl;

  return kafkaTopic;
}

void KafkaClient::send(const void* msg, int msgLen, const std::string& networkKey,
                       bool onNetworkTopic, bool onProcessTopic, bool onFileTopic) {
  char cidKey[SHORT_CONTAINER_ID_LENGTH];
  int cidKeyLen = std::min(static_cast<int>(sizeof(cidKey)), msgLen - SHORT_CONTAINER_ID_OFFSET);
  if (cidKeyLen < 0) {
    std::cerr << "Invalid message payload: size " << msgLen << " less than " << SHORT_CONTAINER_ID_OFFSET
              << ", could not determine a short container ID" << std::endl;
    return;  // Invalid payload
  }
  strncpy(cidKey, (const char*)msg + SHORT_CONTAINER_ID_OFFSET, cidKeyLen);

  // TODO: @jay stop sending process signals on default topic when filters are retired
  if (defaultTopicHandle) {
    sendMessage(defaultTopicHandle, msg, msgLen, cidKey, cidKeyLen);
  }
  if (onNetworkTopic && networkTopicHandle && !networkKey.empty()) {
    sendMessage(networkTopicHandle, msg, msgLen, (char*)networkKey.c_str(), networkKey.size());
  }
  if (onProcessTopic && processTopicHandle) {
    sendMessage(processTopicHandle, msg, msgLen, cidKey, cidKeyLen);
  }
  if (onFileTopic && fileTopicHandle) {
    sendMessage(fileTopicHandle, msg, msgLen, cidKey, cidKeyLen);
  }
}

void KafkaClient::sendMessage(rd_kafka_topic_t* kafkaTopic, const void* msg, int msgLen, char* key, int keyLen) {

  // rd_kafka_produce doesn't modify the payload if RD_KAFKA_MSG_F_COPY is specified, so the const_cast is safe.
  int rv = rd_kafka_produce(kafkaTopic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
       const_cast<void*>(msg), msgLen, key, keyLen, NULL);
  if (rv == -1) {
    fprintf(stderr, "Failed to produce to topic %s: %s\n",
                    rd_kafka_topic_name(kafkaTopic),
                    rd_kafka_err2str(rd_kafka_errno2err(errno)));
  }
  rd_kafka_poll(kafka, 0);
}
