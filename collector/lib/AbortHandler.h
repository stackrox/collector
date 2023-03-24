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
#ifndef _ABORT_HANDLER_H_
#define _ABORT_HANDLER_H_

#include <csignal>
#include <cstdio>
#include <cstdlib>

extern "C" {

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <string.h>
#include <unistd.h>
}

#include "Utility.h"

// Standards before C++17 [1] specify that a signal handler installed via
// std::signal should have a C language linkage, otherwise resulting in
// undefined behavior. We use C++17, which relax this to only being
// signal-safe, plus it's not clear to me if using std::signal and ANSI C
// signal is equivalent in this sense, but it probably wont hurt to stick to C
// linkage anyway.
//
// [1]: https://en.cppreference.com/w/cpp/utility/program/signal
extern "C" void AbortHandler(int signum);

#endif  // _ABORT_H_
