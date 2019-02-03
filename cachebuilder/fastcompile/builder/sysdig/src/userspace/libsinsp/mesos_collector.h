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

//
// mesos_collector.h
//

#pragma once

#ifdef HAS_CAPTURE

#include "mesos_common.h"
#include <map>
#include <memory>

class mesos_http;

class mesos_collector
{
public:
	typedef std::map<int, std::shared_ptr<mesos_http>> socket_map_t;

	mesos_collector(bool do_loop = true, long timeout_ms = 1000L);

	~mesos_collector();

	void add(std::shared_ptr<mesos_http> handler);

	void remove_all();

	int subscription_count() const;

	void get_data();

	void stop();

	bool is_active() const;
	bool is_healthy(int expected_count) const;

	bool has(std::shared_ptr<mesos_http> handler);
	bool remove(std::shared_ptr<mesos_http> handler);

private:
	void clear();
	socket_map_t::iterator& remove(socket_map_t::iterator& it);

	socket_map_t     m_sockets;
	fd_set           m_infd;
	fd_set           m_errfd;
	int              m_nfds;
	bool             m_loop;
	long             m_timeout_ms;
	bool             m_stopped;
};

inline void mesos_collector::stop()
{
	m_stopped = true;
}

#endif // HAS_CAPTURE
