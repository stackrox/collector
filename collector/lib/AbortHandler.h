
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
