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
// k8s_common.h
//

#pragma once

//
// Macros for enable/disable k8s threading
// Used to eliminate mutex locking when running single-threaded
//

#ifndef K8S_DISABLE_THREAD
#ifdef HAS_ANALYZER
#warning "K8S watch in thread is experimental"
#else
#error "K8S watch in thread not supported. Please #define K8S_DISABLE_THREAD"
#endif // HAS_ANALYZER
#include <mutex>
#define K8S_DECLARE_MUTEX mutable std::mutex m_mutex
#define K8S_LOCK_GUARD_MUTEX std::lock_guard<std::mutex> lock(m_mutex)
#else
#define K8S_DECLARE_MUTEX
#define K8S_LOCK_GUARD_MUTEX
#endif // K8S_DISABLE_THREAD

