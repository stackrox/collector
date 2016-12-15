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

////////////////////////////////////////////////////////////////////////////
// Public definitions for the scap library
////////////////////////////////////////////////////////////////////////////
#include "sinsp.h"
#include "sinsp_int.h"

#ifdef GATHER_INTERNAL_STATS

void sinsp_stats::clear()
{
	m_n_seen_evts = 0;
	m_n_drops = 0;
	m_n_preemptions = 0;
	m_n_noncached_fd_lookups = 0;
	m_n_cached_fd_lookups = 0;
	m_n_failed_fd_lookups = 0;
	m_n_threads = 0;
	m_n_fds = 0;
	m_n_added_fds = 0;
	m_n_removed_fds = 0;
	m_n_stored_evts = 0;
	m_n_store_drops = 0;
	m_n_retrieved_evts = 0;
	m_n_retrieve_drops = 0;
	m_metrics_registry.clear_all_metrics();
}

void sinsp_stats::emit(FILE* f)
{
	m_output_target = f;

	fprintf(f, "evts seen by driver: %" PRIu64 "\n", m_n_seen_evts);
	fprintf(f, "drops: %" PRIu64 "\n", m_n_drops);
	fprintf(f, "preemptions: %" PRIu64 "\n", m_n_preemptions);
	fprintf(f, "fd lookups: %" PRIu64 "(%" PRIu64 " cached %" PRIu64 " noncached)\n", 
		m_n_noncached_fd_lookups + m_n_cached_fd_lookups,
		m_n_cached_fd_lookups,
		m_n_noncached_fd_lookups);
	fprintf(f, "failed fd lookups: %" PRIu64 "\n", m_n_failed_fd_lookups);
	fprintf(f, "n. threads: %" PRIu64 "\n", m_n_threads);
	fprintf(f, "n. fds: %" PRIu64 "\n", m_n_fds);
	fprintf(f, "added fds: %" PRIu64 "\n", m_n_added_fds);
	fprintf(f, "removed fds: %" PRIu64 "\n", m_n_removed_fds);
	fprintf(f, "stored evts: %" PRIu64 "\n", m_n_stored_evts);
	fprintf(f, "store drops: %" PRIu64 "\n", m_n_store_drops);
	fprintf(f, "retrieved evts: %" PRIu64 "\n", m_n_retrieved_evts);
	fprintf(f, "retrieve drops: %" PRIu64 "\n", m_n_retrieve_drops);

	for(internal_metrics::registry::metric_map_iterator_t it = m_metrics_registry.get_metrics().begin(); it != m_metrics_registry.get_metrics().end(); it++)
	{
		fprintf(f, "%s: ", it->first.get_description().c_str());
		it->second->process(*this);
	}
}

void sinsp_stats::process(internal_metrics::counter& metric)
{
	fprintf(m_output_target, "%" PRIu64 "\n", metric.get_value());
}

#endif // GATHER_INTERNAL_STATS
