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

#define SE_NSIG           64
#define SE_SIGHUP           1
#define SE_SIGINT           2
#define SE_SIGQUIT          3
#define SE_SIGILL           4
#define SE_SIGTRAP          5
#define SE_SIGABRT          6
#define SE_SIGIOT           6
#define SE_SIGBUS           7
#define SE_SIGFPE           8
#define SE_SIGKILL          9
#define SE_SIGUSR1         10
#define SE_SIGSEGV         11
#define SE_SIGUSR2         12
#define SE_SIGPIPE         13
#define SE_SIGALRM         14
#define SE_SIGTERM         15
#define SE_SIGSTKFLT       16
#define SE_SIGCHLD         17
#define SE_SIGCONT         18
#define SE_SIGSTOP         19
#define SE_SIGTSTP         20
#define SE_SIGTTIN         21
#define SE_SIGTTOU         22
#define SE_SIGURG          23
#define SE_SIGXCPU         24
#define SE_SIGXFSZ         25
#define SE_SIGVTALRM       26
#define SE_SIGPROF         27
#define SE_SIGWINCH        28
#define SE_SIGIO           29
#define SE_SIGPOLL         SE_SIGIO
#define SE_SIGPWR          30
#define SE_SIGSYS          31
#define SE_SIGUNUSED       31
