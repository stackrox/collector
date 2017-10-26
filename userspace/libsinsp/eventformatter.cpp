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

#include "sinsp.h"
#include "sinsp_int.h"
#include "filter.h"
#include "filterchecks.h"
#include "eventformatter.h"

///////////////////////////////////////////////////////////////////////////////
// rawstring_check implementation
///////////////////////////////////////////////////////////////////////////////
#ifdef HAS_FILTERING
extern sinsp_filter_check_list g_filterlist;

sinsp_evt_formatter::sinsp_evt_formatter(sinsp* inspector, const string& fmt)
{
	m_inspector = inspector;

	string sysdigFormat = "*";
	string prefix = "%";
	vector<string> fields;
	char* str = new char[fmt.length() + 1];
	strcpy(str, fmt.c_str());
	char* token = strtok(str, ",");
	while (token != NULL) {
		fields.push_back(token);
		token = strtok(NULL, ",");
	}
	delete[] str;
	for (vector<string>::iterator it = fields.begin();
	     it != fields.end(); it++)
	{
		token = strtok((char*)(*it).c_str(), ":");
		string mappedValue = token;
		token = strtok(NULL, ":");
		string field = token;
		sysdigFormat += (prefix + field + " ");
		m_fields_map[field] = mappedValue;
	}

	cout << "sysdig format: " << sysdigFormat << endl;
	for (map<string, string>::iterator it = m_fields_map.begin();
	     it != m_fields_map.end(); it++)
	{
		cout << it->first << " -> " << it->second << endl;
	}

	set_format(sysdigFormat);
}

sinsp_evt_formatter::~sinsp_evt_formatter()
{
	uint32_t j;

	for(j = 0; j < m_chks_to_free.size(); j++)
	{
		delete m_chks_to_free[j];
	}
}

void sinsp_evt_formatter::set_format(const string& fmt)
{
	uint32_t j;
	uint32_t last_nontoken_str_start = 0;
	string lfmt(fmt);

	if(lfmt == "")
	{
		throw sinsp_exception("empty formatting token");
	}

	//
	// If the string starts with a *, it means that we are ok with printing
	// the string even when not all the values it specifies are set.
	//
	if(lfmt[0] == '*')
	{
		m_require_all_values = false;
		lfmt.erase(0, 1);
	}
	else
	{
		m_require_all_values = true;
	}

	//
	// Parse the string and extract the tokens
	//
	const char* cfmt = lfmt.c_str();

	m_tokens.clear();
	uint32_t lfmtlen = (uint32_t)lfmt.length();

	for(j = 0; j < lfmtlen; j++)
	{
		if(cfmt[j] == '%')
		{
			int toklen = 0;

			if(last_nontoken_str_start != j)
			{
				rawstring_check* newtkn = new rawstring_check(lfmt.substr(last_nontoken_str_start, j - last_nontoken_str_start));
				m_tokens.push_back(newtkn);
				m_tokenlens.push_back(0);
				m_chks_to_free.push_back(newtkn);
				// cout << "New token: " << lfmt.substr(last_nontoken_str_start, j - last_nontoken_str_start) << ", nTokens = " << to_string(m_tokens.size()) << endl;
			}

			if(j == lfmtlen - 1)
			{
				throw sinsp_exception("invalid formatting syntax: formatting cannot end with a %");
			}

			//
			// If the field specifier starts with a number, it means that we have a length modifier
			//
			if(isdigit(cfmt[j + 1]))
			{
				//
				// Parse the token length
				//
				sscanf(cfmt+ j + 1, "%d", &toklen);

				//
				// Advance until the beginning of the field name
				//
				while(true)
				{
					if(j == lfmtlen - 1)
					{
						throw sinsp_exception("invalid formatting syntax: formatting cannot end with a number");
					}
					else if(isdigit(cfmt[j + 1]))
					{
						j++;
						continue;
					}
					else
					{
						break;
					}
				}
			}

			sinsp_filter_check* chk = g_filterlist.new_filter_check_from_fldname(string(cfmt + j + 1),
				m_inspector,
				false);
			// Begin StackRox section
			// cout << "Parsing: " << string(cfmt + j + 1) << ", nTokens = " << to_string(m_tokens.size()) << endl;
			char field_name[64] = "<Unknown>";
			const char* cptr = cfmt + j + 1;
			for (unsigned int i = 0; i < strlen(cfmt + j + 1); i++) {
				if (cptr[i] != ' ')
				{
					field_name[i] = cptr[i];
				}
				else {
					field_name[i] = '\0';
					break;
				}
			}

			m_token_to_field_map[m_tokens.size()] = field_name;
			// End StackRox section

			if(chk == NULL)
			{
				throw sinsp_exception("invalid formatting token " + string(cfmt + j + 1));
			}

			m_chks_to_free.push_back(chk);

			j += chk->parse_field_name(cfmt + j + 1, true, false);
			ASSERT(j <= lfmt.length());

			m_tokens.push_back(chk);
			m_tokenlens.push_back(toklen);

			last_nontoken_str_start = j + 1;
		}
	}

	if(last_nontoken_str_start != j)
	{
		sinsp_filter_check * chk = new rawstring_check(lfmt.substr(last_nontoken_str_start, j - last_nontoken_str_start));
		m_tokens.push_back(chk);
		m_chks_to_free.push_back(chk);
		m_tokenlens.push_back(0);
		// cout << "Final new token: " << lfmt.substr(last_nontoken_str_start, j - last_nontoken_str_start) << ", nTokens = " << to_string(m_tokens.size()) << endl;
	}
}

bool sinsp_evt_formatter::on_capture_end(OUT string* res)
{
	res->clear();
	return res->size() > 0;
}

bool sinsp_evt_formatter::resolve_tokens(sinsp_evt *evt, map<string,string>& values)
{
	bool retval = true;
	const filtercheck_field_info* fi;
	uint32_t j = 0;

	ASSERT(m_tokenlens.size() == m_tokens.size());

	for(j = 0; j < m_tokens.size(); j++)
	{
		char* str = m_tokens[j]->tostring(evt);

		if(str == NULL)
		{
			if(m_require_all_values)
			{
				retval = false;
				break;
			}
			else
			{
				str = (char*)"<NA>";
			}
		}

		fi = m_tokens[j]->get_field_info();
		if(fi)
		{
			values[fi->m_name] = string(str);
		}
	}

	return retval;
}


bool sinsp_evt_formatter::tostring(sinsp_evt* evt, OUT string* res)
{
	bool retval = true;
	// Begin StackRox section
	/*
	const filtercheck_field_info* fi;
	*/

	uint32_t j = 0;
	vector<sinsp_filter_check*>::iterator it;
	res->clear();

	ASSERT(m_tokenlens.size() == m_tokens.size());

	for(j = 0; j < m_tokens.size(); j++)
	{
		// Begin StackRox section
		/*
		if(m_inspector->get_buffer_format() == sinsp_evt::PF_JSON
		   || m_inspector->get_buffer_format() == sinsp_evt::PF_JSONEOLS
		   || m_inspector->get_buffer_format() == sinsp_evt::PF_JSONHEX
		   || m_inspector->get_buffer_format() == sinsp_evt::PF_JSONHEXASCII
		   || m_inspector->get_buffer_format() == sinsp_evt::PF_JSONBASE64)
		{
			Json::Value json_value = m_tokens[j]->tojson(evt);

			if(retval == false)
			{
				continue;
			}

			if(json_value == Json::nullValue && m_require_all_values)
			{
				retval = false;
				continue;
			}

			fi = m_tokens[j]->get_field_info();

			if(fi)
			{
				m_root[fi->m_name] = m_tokens[j]->tojson(evt);
			}
		}
		else
		{
		*/
		// End StackRox section
			char* str = m_tokens[j]->tostring(evt);

			if(retval == false)
			{
				continue;
			}

			if(str == NULL)
			{
				if(m_require_all_values)
				{
					retval = false;
					continue;
				}
				else
				{
					str = (char*)"<NA>";
				}
			}

			uint32_t tks = m_tokenlens[j];

			if(tks != 0)
			{
				string sstr(str);
				sstr.resize(tks, ' ');
				(*res) += sstr;
			}
			else
			{
				(*res) += str;
			}
		//}
	}

	// Begin StackRox section
	/*
	if(m_inspector->get_buffer_format() == sinsp_evt::PF_JSON
	   || m_inspector->get_buffer_format() == sinsp_evt::PF_JSONEOLS
	   || m_inspector->get_buffer_format() == sinsp_evt::PF_JSONHEX
	   || m_inspector->get_buffer_format() == sinsp_evt::PF_JSONHEXASCII
	   || m_inspector->get_buffer_format() == sinsp_evt::PF_JSONBASE64)
	{
		(*res) = "\n";
		(*res) += m_writer.write(m_root);
		(*res) = res->substr(0, res->size() - 1);
	}
	*/
	// End StackRox section

	return retval;
}

// Begin StackRox section
SignalType sinsp_evt_formatter::to_sparse_string(sinsp_evt* evt, char* buffer, unsigned int snaplen, string& network_key)
{
	SignalType signalType = SIGNAL_TYPE_DEFAULT;

	uint32_t j = 0;

	ASSERT(m_tokenlens.size() == m_tokens.size());

	network_key.clear();
	buffer[0] = 'S';
	buffer[1] = '\0';
	char* ptr = &buffer[1];
	bool past_fixed_format = false;

	// m_token_to_field_map has [index in m_tokens] -> [field name]
	for(map<int, string>::iterator it = m_token_to_field_map.begin();
			it != m_token_to_field_map.end(); it++)
	{
		j = it->first;
		if (j >= m_tokens.size())
		{
			continue;
		}

		// m_fields_map is set via the format string n:evt.field1,n+1:evt.field2...
		// keys are field names; values are integer codes
		map<string, string>::iterator f = m_fields_map.find(it->second);
		if (f == m_fields_map.end())
		{
			continue;
		}
		string mapped_value = f->second; // output format code

		char* str = m_tokens[j]->tostring(evt); // the field value
		char* field = (char*)it->second.c_str(); // the field name
		if (str != NULL)
		{
			if (str[0] == '\0')
			{
				continue;
			}

			if (buffer[0] != 'N')
			{
				if (!strcmp(field, "evt.category") && !strcmp(str, "net"))
				{
					buffer[0] = 'N';
                    			signalType = SIGNAL_TYPE_NETWORK;
				}
				else if (!strcmp(field, "fd.type") &&
					(!strcmp(str, "ipv4") || !strcmp(str, "ipv6") || !strcmp(str, "unix")))
				{
					buffer[0] = 'N';
                   			signalType = SIGNAL_TYPE_NETWORK;
				}
			}
            
			if (!strcmp(field, "evt.type"))
			{
				if (m_process_evttypes.end() != m_process_evttypes.find(str))
				{
					signalType = SIGNAL_TYPE_PROCESS;
				}
			}

			if (past_fixed_format)
			{
				if (!strcmp(field, "fd.cip") || !strcmp(field, "fd.cport"))
				{
					network_key += str;
				}

				if (!strcmp(field, "proc.args") || !strcmp(field, "proc.cmdline") || !strcmp(field, "proc.exeline"))
				{
					// if there are tabs in proc.args, proc.cmdline, or proc.exeline,
					// then replace them with periods
					char* t = str;
					while (*t != '\0')
					{
						if (*t == 9)
						{
							*t = 46;
						}
						t++;
					}
				}
				else if (!strcmp(field, "evt.args") && buffer[0] == 'N')
				{
					// for network data remove anything that comes after "data=" as this is
					// already part of the buffer
					char* t = str;
					while (*t != '\0')
					{
						if (*t == 'd' && *(t + 1) == 'a' && *(t + 2) == 't' && *(t + 3) == 'a' && *(t + 4) == '=')
						{
							*t = '\0';
							break;
						}
						t++;
					}
				}
				// truncate to snaplen if over
				if (strlen(str) > snaplen)
				{
					str[snaplen] = '\0';
				}
				sprintf(ptr, "\t%s:%s", mapped_value.c_str(), str);
				ptr += strlen(ptr);
			}
			else
			{
				sprintf(ptr, "\t%s", str);
				ptr += strlen(ptr);
				if (!strcmp(field, "evt.time"))
				{
					past_fixed_format = true;
				}
			}
		}
		else if (!strcmp(field, "container.name") || !strcmp(field, "container.image"))
		{
			buffer[0] = '\0';
			return signalType;
		}
	}

	return signalType;
}

void sinsp_evt_formatter::init_process_syscalls(const string& process_syscalls) {
    istringstream is(process_syscalls);
    string syscall;
    while (getline(is, syscall, ','))
    {
        m_process_evttypes.insert(syscall);
    }
}

// End StackRox section

#else  // HAS_FILTERING

sinsp_evt_formatter::sinsp_evt_formatter(sinsp* inspector, const string& fmt)
{
}

void sinsp_evt_formatter::set_format(const string& fmt)
{
	throw sinsp_exception("sinsp_evt_formatter unvavailable because it was not compiled in the library");
}

bool sinsp_evt_formatter::resolve_tokens(sinsp_evt *evt, map<string,string>& values)
{
	throw sinsp_exception("sinsp_evt_formatter unvavailable because it was not compiled in the library");
}

bool sinsp_evt_formatter::tostring(sinsp_evt* evt, OUT string* res)
{
	throw sinsp_exception("sinsp_evt_formatter unvavailable because it was not compiled in the library");
}
#endif // HAS_FILTERING

sinsp_evt_formatter_cache::sinsp_evt_formatter_cache(sinsp *inspector)
	: m_inspector(inspector)
{
}

sinsp_evt_formatter_cache::~sinsp_evt_formatter_cache()
{
}

std::shared_ptr<sinsp_evt_formatter>& sinsp_evt_formatter_cache::get_cached_formatter(string &format)
{
	auto it = m_formatter_cache.lower_bound(format);

	if(it == m_formatter_cache.end() ||
	   it->first != format)
	{
		it = m_formatter_cache.emplace_hint(it,
						    std::make_pair(format, make_shared<sinsp_evt_formatter>(m_inspector, format)));
	}

	return it->second;
}

bool sinsp_evt_formatter_cache::resolve_tokens(sinsp_evt *evt, string &format, map<string,string>& values)
{
	return get_cached_formatter(format)->resolve_tokens(evt, values);
}

bool sinsp_evt_formatter_cache::tostring(sinsp_evt *evt, string &format, OUT string *res)
{
	return get_cached_formatter(format)->tostring(evt, res);
}
