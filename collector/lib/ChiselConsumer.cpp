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

#include "ChiselConsumer.h"

#include <iostream>
#include <mutex>
#include <sstream>

#include "librdkafka/rdkafka.h"

#include "Logging.h"

namespace collector {

void ChiselConsumer::handleChisel(const rd_kafka_message_t* message) {
  chisel_update_cb_(std::string((const char *)message->payload, message->len));
}

void ChiselConsumer::processMessage(const rd_kafka_message_t* message) {
    rd_kafka_resp_err_t err = message ? message->err : rd_kafka_last_error();
    switch (err) {
        case RD_KAFKA_RESP_ERR__TIMED_OUT:
            // "Operation timed out", an internal rdkafka error.
            // This seems to occur whenever a consume() returns
            // at the configured timeout.
            break;

        case RD_KAFKA_RESP_ERR_NO_ERROR:
            /* Real message */
            handleChisel(message);
            break;

        case RD_KAFKA_RESP_ERR__PARTITION_EOF:
            // "Reached the end of the topic+partition queue on the broker. Not really an error."
            //    - rdkafkacpp.h
            break;

        default:
            CLOG(ERROR) << "Consume failed: " << rd_kafka_err2str(err);
    }
}

void ChiselConsumer::Start() {
  StoppableThread::Start([this]{runForever();});
}

void ChiselConsumer::runForever() {
    // See https://github.com/edenhill/librdkafka/blob/master/examples/rdkafka_consumer_example.cpp
    // for an example of how to use the rdkafka C++ library.

    char errstr[256];

    rd_kafka_conf_t* conf = rd_kafka_conf_dup(conf_template_);

    std::stringstream ss;
    ss << unique_name_ << "-" << std::time(nullptr);
    rd_kafka_conf_res_t res = rd_kafka_conf_set(conf, "group.id", ss.str().c_str(), errstr, sizeof(errstr));
    if (res != RD_KAFKA_CONF_OK) {
        CLOG(FATAL) << "Failed to set chisel consumer group ID: " << errstr;
    }

    rd_kafka_t* consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!consumer) {
        CLOG(FATAL) << "Failed to create chisel consumer: " << errstr;
    }

    CLOG(INFO) << "% Created chisel consumer " << rd_kafka_name(consumer);

    // Subscribe to topic
    rd_kafka_topic_partition_list_t* topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics, topic_.c_str(), RD_KAFKA_PARTITION_UA);
    rd_kafka_resp_err_t err = rd_kafka_subscribe(consumer, topics);
    rd_kafka_topic_partition_list_destroy(topics);

    if (err) {
        CLOG(FATAL) << "Failed to subscribe to topic '" << topic_ << "' for chisel consumer: "
                    << rd_kafka_err2str(err);
    }

    // Consume messages
    while (!should_stop()) {
        rd_kafka_message_t* msg = rd_kafka_consumer_poll(consumer, 1000);
        processMessage(msg);
        if (msg) rd_kafka_message_destroy(msg);
    }

    // Stop consumer
    err = rd_kafka_consumer_close(consumer);
    if (err) {
        CLOG(ERROR) << "Failed to close chisel consumer: " << rd_kafka_err2str(err);
    }
    rd_kafka_destroy(consumer);
}

}  // namespace collector
