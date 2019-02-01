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
// marathon_http.cpp
//

#ifdef HAS_CAPTURE

#include "marathon_http.h"
#include "curl/curl.h"
#include "curl/easy.h"
#define BUFFERSIZE 512 // b64 needs this macro
#include "b64/encode.h"
#include "sinsp.h"
#include "sinsp_int.h"
#include "json_error_log.h"
#include "mesos.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

marathon_http::marathon_http(mesos& m, const uri& url, bool discover_marathon, int timeout_ms, const string& token):
	mesos_http(m, url, false, discover_marathon, timeout_ms, token)
{
	g_logger.log("Creating Marathon HTTP object for [" + url.to_string(false) + "] ...", sinsp_logger::SEV_DEBUG);
	if(refresh_data())
	{
		g_logger.log("Created Marathon HTTP connection (" + url.to_string(false) + ") for framework [" +
					 get_framework_name() + "] (" + get_framework_id() + "), version: " + get_framework_version(),
					 sinsp_logger::SEV_INFO);
	}
	else
	{
		throw sinsp_exception("Could not obtain Mesos Marathon framework information.");
	}

	g_logger.log("Marathon request [" + get_request() + ']', sinsp_logger::SEV_DEBUG);
}

marathon_http::~marathon_http()
{
}

bool marathon_http::refresh_data()
{
	std::ostringstream os;
	std::string uri = make_uri("/v2/info");
	CURLcode res = get_data(uri, os);

	if(res != CURLE_OK)
	{
		std::string errstr = std::string("Problem accessing /v2/info: ") + curl_easy_strerror(res);
		g_logger.log(errstr, sinsp_logger::SEV_ERROR);
		g_json_error_log.log(os.str(), errstr, sinsp_utils::get_current_time_ns(), uri);
		return false;
	}

	try
	{
		Json::Value root;
		Json::Reader reader;
		if(reader.parse(os.str(), root, false))
		{
			set_framework_id(get_json_string(root, "frameworkId"));
			set_framework_name(get_json_string(root, "name"));
			set_framework_version(get_json_string(root, "version"));
			g_logger.log("Found Marathon framework: " + get_framework_name() + " (" + get_framework_id() + "), version: " + get_framework_version(), sinsp_logger::SEV_DEBUG);
		}
		else
		{
			std::string errstr;
			errstr = reader.getFormattedErrorMessages();
			g_logger.log("Error parsing framework info (" + errstr + ").\nJSON:\n---\n" + os.str() + "\n---", sinsp_logger::SEV_ERROR);
			g_json_error_log.log(os.str(), errstr, sinsp_utils::get_current_time_ns(), uri);
			return false;
		}
	}
	catch(std::exception& ex)
	{
		std::string errstr = std::string("Error parsing framework info:") + ex.what();
		g_logger.log(errstr, sinsp_logger::SEV_ERROR);
		g_json_error_log.log(os.str(), errstr, sinsp_utils::get_current_time_ns(), uri);
		return false;
	}
	
	return true;
}

std::string marathon_http::get_groups(const std::string& group_id)
{
	std::ostringstream os;
	std::string uri = make_uri("/v2/groups" + group_id);
	CURLcode res = get_data(uri, os);

	if(res != CURLE_OK)
	{
		std::string errstr = std::string("Problem accessing /v2/groups: ") + curl_easy_strerror(res);
		g_logger.log(errstr, sinsp_logger::SEV_ERROR);
		g_json_error_log.log(os.str(), errstr, sinsp_utils::get_current_time_ns(), uri);
		return "";
	}

	return os.str();
}

#endif // HAS_CAPTURE
