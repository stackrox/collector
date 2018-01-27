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

/*
Copyright (C) 2013-2014 Draios inc.

This file is part of sysdig.

sysdig is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

sysdig is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sysdig.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <config_sysdig.h>
#ifdef HAS_CAPTURE
#include "../../driver/driver_config.h"
#endif // HAS_CAPTURE

//
// ASSERT implementation
//
#ifdef _DEBUG
#define ASSERT(X) assert(X)
#else // _DEBUG
#define ASSERT(X)
#endif // _DEBUG

#include "KafkaClient.h"
#include "safe_buffer.h"

//
// Capture results
//
class sysdig_init_res
{
public:
	sysdig_init_res()
	{
		m_res = EXIT_SUCCESS;
	}

	sysdig_init_res(int res)
	{
		m_res = res;
	}

	int m_res;
	vector<string> m_next_run_args;
};

//
// Capture results
//
class captureinfo
{
public:
	captureinfo()
	{
		m_nevts = 0;
		m_time = 0;
	}

	uint64_t m_nevts;
	uint64_t m_time;
};

//
// Summary table entry
//
class summary_table_entry
{
public:
	summary_table_entry(uint16_t id, bool is_unsupported_syscall) : m_ncalls(0), m_id(id), m_is_unsupported_syscall(is_unsupported_syscall)
	{
	}

	uint64_t m_ncalls;
	uint16_t m_id;
	bool m_is_unsupported_syscall;	
};

struct summary_table_entry_rsort_comparer
{
    bool operator() (const summary_table_entry& first, const summary_table_entry& second) const
	{
		return first.m_ncalls > second.m_ncalls;
	}
};

#define DEFAULT_BUFFER_SIZE 4096
#define DEFAULT_METRICS_PERIODICITY 2000000

class SysdigPersistentState {
    private:
        long metricsFrequency;
        sinsp* inspector;
        string chiselName;
        sinsp_chisel* chisel;
        sinsp_evt_formatter* formatter;
        int periodicity;
        int snapLen;
        SafeBuffer eventBuffer;
        bool useKafka;
        KafkaClient* kafkaClient;

    public:
        SysdigPersistentState(long _metricsFrequency, sinsp* _inspector, string _chiselName, sinsp_chisel* _chisel,
                              sinsp_evt_formatter* _formatter, int _periodicity, int _snapLen,
                              int eventBufferSize, bool _useKafka, string brokerList, string defaultTopic,
                              string networkTopic, string processTopic, string processSyscalls) :
            metricsFrequency(_metricsFrequency),
            inspector(_inspector),
            chiselName(_chiselName),
            chisel(_chisel),
            formatter(_formatter),
            periodicity(_periodicity),
            snapLen(_snapLen),
            eventBuffer(eventBufferSize <= 0 ? DEFAULT_BUFFER_SIZE : eventBufferSize),
            useKafka(_useKafka)
            {
            kafkaClient = NULL;
            if (_useKafka) {
                kafkaClient = new KafkaClient(brokerList, defaultTopic, networkTopic, processTopic);
            }
        }
        SysdigPersistentState(int eventBufferSize, bool _useKafka, string brokerList, string defaultTopic,
                              string networkTopic, string processTopic, string processSyscalls)
                : eventBuffer(eventBufferSize <= 0 ? DEFAULT_BUFFER_SIZE : eventBufferSize) {
            useKafka = _useKafka;
            kafkaClient = NULL;
            if (_useKafka) {
                kafkaClient = new KafkaClient(brokerList, defaultTopic, networkTopic, processTopic);
            }
        }
        SysdigPersistentState() : eventBuffer(DEFAULT_BUFFER_SIZE) {
            useKafka = false;
            kafkaClient = NULL;
        }

        virtual ~SysdigPersistentState() {
            delete kafkaClient;
        }

        inline long getMetricsFrequency() { return metricsFrequency; }
        inline sinsp* getInspector() { return inspector; }
        inline string getChiselName() { return chiselName; }
        inline sinsp_chisel* getChisel() { return chisel; }
        inline sinsp_evt_formatter* getFormatter() { return formatter; }
        inline int getPeriodicity() { return periodicity; }
        inline int getSnapLen() { return snapLen; }
        inline SafeBuffer& getEventBuffer() { return eventBuffer; }
        inline bool usingKafka() { return useKafka; }
        inline KafkaClient* getKafkaClient() { return kafkaClient; }

        inline void setMetricsFrequency(long _metricsFrequency) { metricsFrequency = _metricsFrequency; }
        inline void setInspector(sinsp* _inspector) { inspector = _inspector; }
        inline void setChiselName(string _chiselName) { chiselName = _chiselName; }
        inline void setChisel(sinsp_chisel* _chisel) { chisel = _chisel; }
        inline void setFormatter(sinsp_evt_formatter* _formatter) { formatter = _formatter; }
        inline void setPeriodicity(int _periodicity) { periodicity = _periodicity; }
        inline void setSnapLen(int _snapLen) { snapLen = _snapLen; }
        inline void setUseKafka(bool _useKafka) { useKafka = _useKafka; }
        inline void setEventBufferSize(int eventBufferSize) {
            eventBuffer.reset(eventBufferSize <= 0 ? DEFAULT_BUFFER_SIZE : eventBufferSize);
        }
        inline void setupKafkaClient(string brokerList, string defaultTopic, string networkTopic,
                                     string processTopic) {
            kafkaClient = new KafkaClient(brokerList, defaultTopic, networkTopic, processTopic);
        }
};

extern "C" {

typedef struct {
    uint64_t    nEvents;                // the number of kernel events
    uint64_t    nDrops;                 // the number of drops
    uint64_t    nPreemptions;           // the number of preemptions
    uint64_t    nFilteredEvents;        // events post chisel filter
    std::string nodeName;               // the name of this node (hostname)
} sysdigDataT;

int sysdigInitialize(string chiselName, string brokerList, string format, bool useKafka,
                     string defaultTopic, string networkTopic, string processTopic, string processSyscalls, int snapLen);
void sysdigCleanup();
void sysdigStartProduction(bool& isInterrupted);
bool sysdigGetSysdigData(sysdigDataT& sysdigData);
bool isSysdigInitialized();

}

sysdig_init_res sysdig_init(int argc, char** argv, bool setupOnly = false);

//
// Printer functions
//
void list_fields(bool verbose, bool markdown);
void list_events(sinsp* inspector);

#ifdef HAS_CHISELS
void print_chisel_info(chisel_desc* cd);
void list_chisels(vector<chisel_desc>* chlist, bool verbose);
#endif
