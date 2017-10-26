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
#include <json/json.h>

class sinsp_filter_check;

typedef enum signalType {
    SIGNAL_TYPE_DEFAULT = 0,
    SIGNAL_TYPE_NETWORK = 1,
    SIGNAL_TYPE_PROCESS = 2
} SignalType;

/** @defgroup event Event manipulation
 *  @{
 */

/*!
  \brief Event to string converter class.
  This class can be used to format an event into a string, based on an arbitrary
  format.
*/
class SINSP_PUBLIC sinsp_evt_formatter
{
public:
	/*!
	  \brief Constructs a formatter.

	  \param inspector Pointer to the inspector instance that will generate the
	   events to be formatter.
	  \param fmt The printf-like format to use. The accepted format is the same
	   as the one of the sysdig '-p' command line flag, so refer to the sysdig
	   manual for details.
      \param processSyscalls a comma-delimited set of event types that represent
       process syscalls
	*/
	sinsp_evt_formatter(sinsp* inspector, const string& fmt);

	~sinsp_evt_formatter();

	/*!
	  \brief Resolve all the formatted tokens and return them in a key/value
	  map.

	  \param evt Pointer to the event to be converted into string.
	  \param res Reference to the map that will be filled with the result.

	  \return true if all the tokens can be retrieved successfully, false
	  otherwise.
	*/
	bool resolve_tokens(sinsp_evt *evt, map<string,string>& values);

	/*!
	  \brief Fills res with the string rendering of the event.

	  \param evt Pointer to the event to be converted into string.
	  \param res Pointer to the string that will be filled with the result.

	  \return true if the string should be shown (based on the initial *),
	   false otherwise.
	*/
	bool tostring(sinsp_evt* evt, OUT string* res);

	/*!
	  \brief Fills res with the string rendering of the event. Variant
	  to do sparse vector rendering of the event

	  \param evt Pointer to the event to be converted into string.
	  \param res Pointer to the string that will be filled with the result.

	  \return true if the string should be shown (based on the initial *),
	   false otherwise.
	*/
	SignalType to_sparse_string(sinsp_evt* evt, char* buffer, unsigned int snaplen, string& network_key);

	/*!
	  \brief Fills res with end of capture string rendering of the event.
	  \param res Pointer to the string that will be filled with the result.

	  \return true if there is a string to show (based on the format),
	   false otherwise.
	*/
	bool on_capture_end(OUT string* res);

    /*!
     \brief Initializes event types that represent process syscalls.

     \param processSyscalls a comma-delimited set of event types that represent
     process syscalls
     */
    void init_process_syscalls(const string& process_syscalls);

private:
	void set_format(const string& fmt);
	vector<sinsp_filter_check*> m_tokens;
	vector<uint32_t> m_tokenlens;
	sinsp* m_inspector;
	bool m_require_all_values;
	vector<sinsp_filter_check*> m_chks_to_free;

	map<int, string> m_token_to_field_map;
	map<string, string> m_fields_map;
    unordered_set<string> m_process_evttypes;

	Json::Value m_root;
	Json::FastWriter m_writer;
};

/*!
  \brief Caching version of sinsp_evt_formatter
  This class is a wrapper around sinsp_evt_formatter, maintaining a
  cache of previously seen formatters. It avoids the overhead of
  recreating sinsp_evt_formatter objects for each event.
*/
class SINSP_PUBLIC sinsp_evt_formatter_cache
{
public:
	sinsp_evt_formatter_cache(sinsp *inspector);
	virtual ~sinsp_evt_formatter_cache();

	// Resolve the tokens inside format and return them as a key/value map.
	// Creates a new sinsp_evt_formatter object if necessary.
	bool resolve_tokens(sinsp_evt *evt, std::string &format, map<string,string>& values);

	// Fills in res with the event formatted according to
	// format. Creates a new sinsp_evt_formatter object if
	// necessary.
	bool tostring(sinsp_evt *evt, std::string &format, OUT std::string *res);

private:

	// Get the formatter for this format string. Creates a new
	// sinsp_evt_formatter object if necessary.
	std::shared_ptr<sinsp_evt_formatter>& get_cached_formatter(string &format);

	std::map<std::string,std::shared_ptr<sinsp_evt_formatter>> m_formatter_cache;
	sinsp *m_inspector;
};
/*@}*/
