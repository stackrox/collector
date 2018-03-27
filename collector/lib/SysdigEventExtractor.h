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

#ifndef _SYSDIG_EVENT_EXTRACTOR_H_
#define _SYSDIG_EVENT_EXTRACTOR_H_

#include "libsinsp/sinsp.h"

namespace collector {

class SysdigEventExtractor {
  public:
    void Init(sinsp* inspector);
    /*
     * General methods for events
     */
    const char* event_type(sinsp_evt* evt);
    const char* event_args(sinsp_evt* evt);
    const char* event_res(sinsp_evt* evt);

    /*
     * Process related extraction methods
     */
    // PID
    const int64_t* get_pid(sinsp_evt* evt);

    // TID
    const int64_t* get_tid(sinsp_evt* evt);

    // Command line
    const std::string* get_comm(sinsp_evt* evt);

    // Exe name
    const std::string* get_exe(sinsp_evt* evt);

    // Exe path
    const std::string* get_exepath(sinsp_evt* evt);

    // Current working dir
    const char* get_cwd(sinsp_evt* evt);

    /*
     * File related extraction methods
     */


    /*
     * Network related extraction methods
     */

    /*
     * Container related extraction methods
     */
    const std::string* container_id(sinsp_evt* evt);

    bool container_privileged(sinsp_evt* evt);

  private:
    // extractor objects
    std::unique_ptr<sinsp_filter_check_iface> filter_check_container_id_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_container_privileged_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_evt_type_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_evt_time_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_evt_rawtime_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_evt_dir_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_evt_buflen_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_evt_io_dir_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_evt_res_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_cip_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_cport_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_l4proto_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_lproto_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_name_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_rip_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_rproto_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_sip_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_sockfamily_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_sport_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_type_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_group_gid_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_proc_cmdline_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_proc_cwd_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_proc_exe_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_proc_exeline_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_proc_name_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_proc_pname_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_user_name_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_user_shell_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_user_uid_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_evt_args_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_proc_pid_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_fd_num_;
    std::unique_ptr<sinsp_filter_check_iface> filter_check_proc_aname_;
};

}  // namespace collector

#endif  // _SYSDIG_EVENT_EXTRACTOR_H_
