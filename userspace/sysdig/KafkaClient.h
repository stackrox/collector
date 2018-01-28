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

extern "C" {
#include "librdkafka/rdkafka.h"
}

#include <string>

using namespace std;

#define SHORT_CONTAINER_ID_LENGTH 12
#define SHORT_CONTAINER_ID_OFFSET 2

class KafkaClient {
 protected:
  rd_kafka_conf_t* conf;
  rd_kafka_conf_res_t res;
  string brokers;
  string defaultTopic;
  string networkTopic;
  string processTopic;
  string fileTopic;

  rd_kafka_t* kafka;
  rd_kafka_topic_t* defaultTopicHandle;
  rd_kafka_topic_t* networkTopicHandle;
  rd_kafka_topic_t* processTopicHandle;
  rd_kafka_topic_t* fileTopicHandle;
  char* containerID;

  // method to create a topic
  rd_kafka_topic_t* createTopic(const char* topic);

  // method to send to a specific topic
  void sendMessage(rd_kafka_topic_t* kafkaTopic, const void* msg, int msgLen, char* key, int keyLen);

 public:
  // construction
  KafkaClient(string brokerList, string defaultTopic, string networkTopic, string processTopic, string fileTopic);
  virtual ~KafkaClient();

  rd_kafka_t* getClient() {
    return kafka;
  }

  const char* const getContainerID() {
      return containerID;
  }

  void send(const void* msg, int msgLen, const string& networkKey,
            bool onNetworkTopic, bool onProcessTopic, bool onFileTopic);
};

#endif // __KAFKACLIENT_H
