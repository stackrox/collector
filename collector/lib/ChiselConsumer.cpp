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

#include <iostream>
#include <mutex>
#include <sstream>

#include "librdkafka/rdkafkacpp.h"

#include "ChiselConsumer.h"

typedef void (*ChiselProcessor)(std::string);

namespace collector {

void
ChiselConsumer::handleChisel(RdKafka::Message* message) {
    this->chisels_received++;
    this->updateChisel(std::string((char *)message->payload()));
}

void
ChiselConsumer::consumeChiselMsg(RdKafka::Message* message, void* opaque) {
    switch (message->err()) {
        case RdKafka::ERR__TIMED_OUT:
            // "Operation timed out", an internal rdkafka error.
            // This seems to occur whenever a consume() returns
            // at the configured timeout.
            break;

        case RdKafka::ERR_NO_ERROR:
            /* Real message */
            handleChisel(message);
            break;

        case RdKafka::ERR__PARTITION_EOF:
            // "Reached the end of the topic+partition queue on the broker. Not really an error."
            //    - rdkafkacpp.h
            break;

        default:
            std::cerr << "Consume failed: " << message->errstr() << std::endl;
    }
}

ChiselConsumer::ChiselConsumer(std::string initial, bool *terminate)
    : contents(initial), terminate(terminate) {
}

bool
ChiselConsumer::updateChisel(std::string newContents) {
    std::lock_guard<std::mutex> contents_guard(this->contents_mutex);

    if (newContents == this->contents) {
        return false;
    }

    this->contents = newContents;
    this->chisel_updates++;
    std::lock_guard<std::mutex> callback_guard(this->callback_mutex);
    if (this->callback != NULL) {
        std::cerr << "Notifying callback of new chisel" << std::endl;
        (*this->callback)(contents);
    }

    return true;
}

int
ChiselConsumer::getChiselsReceived() {
    return this->chisels_received;
}

int
ChiselConsumer::getChiselUpdates() {
    return this->chisel_updates;
}

void
ChiselConsumer::setCallback(ChiselProcessor cb) {
    std::lock_guard<std::mutex> guard(callback_mutex);
    callback = cb;
}

void
ChiselConsumer::runForever(std::string brokerList, std::string topic, std::string uniqueName) {
    // See https://github.com/edenhill/librdkafka/blob/master/examples/rdkafka_consumer_example.cpp
    // for an example of how to use the rdkafka C++ library.

    std::string errstr;

    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    ChiselConsumeCb consume_cb(this);
    RdKafka::Conf::ConfResult res;
    res =conf->set("metadata.broker.list", brokerList, errstr);
    if (res != RdKafka::Conf::ConfResult::CONF_OK) {
        std::cerr << "Failed to set chisel consumer brokers: " << errstr << std::endl;
        exit(1);
    }
    conf->set("consume_cb", &consume_cb, errstr);
    if (res != RdKafka::Conf::ConfResult::CONF_OK) {
        std::cerr << "Failed to set chisel consumer callback: " << errstr << std::endl;
        exit(1);
    }
    std::time_t result = std::time(nullptr);
    std::stringstream ss;
    ss << uniqueName << "-" << result;
    conf->set("group.id", ss.str(), errstr);
    if (res != RdKafka::Conf::ConfResult::CONF_OK) {
        std::cerr << "Failed to set chisel consumer group ID: " << errstr << std::endl;
        exit(1);
    }

    RdKafka::KafkaConsumer *consumer = RdKafka::KafkaConsumer::create(conf, errstr);
    if (!consumer) {
        std::cerr << "Failed to create chisel consumer: " << errstr << std::endl;
        exit(1);
    }

    delete conf;

    std::cerr << "% Created chisel consumer " << consumer->name() << std::endl;

    /*
     * Subscribe to topics
     */
    std::vector<std::string> topics = {topic};
    RdKafka::ErrorCode err = consumer->subscribe(topics);
    if (err) {
        std::cerr << "Failed to subscribe to " << topics.size() << " topic(s) for chisel consumer: "
                  << RdKafka::err2str(err) << std::endl;
        exit(1);
    }

    /*
     * Consume messages
     */
    while (!*this->terminate) {
        RdKafka::Message *msg = consumer->consume(1000);
        this->consumeChiselMsg(msg, NULL);
        delete msg;
    }

    /*
     * Stop consumer
     */
    consumer->close();
    delete consumer;

    /*
     * Wait for RdKafka to decommission.
     * This is not strictly needed (with check outq_len() above), but
     * allows RdKafka to clean up all its resources before the application
     * exits so that memory profilers such as valgrind wont complain about
     * memory leaks.
     */
    RdKafka::wait_destroyed(5000);
}

ChiselConsumeCb::ChiselConsumeCb(ChiselConsumer *chiselConsumer)
    : chiselConsumer(chiselConsumer) {
}

void
ChiselConsumeCb::consume_cb(RdKafka::Message &msg, void *opaque) {
    chiselConsumer->consumeChiselMsg(&msg, opaque);
}

}   /* namespace collector */
