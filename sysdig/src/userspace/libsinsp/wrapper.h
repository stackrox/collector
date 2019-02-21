#ifndef _WRAPPER_H_
#define _WRAPPER_H_

#include <string>

#include "sinsp.h"

sinsp* SINSP_PUBLIC new_inspector();
sinsp_chisel* SINSP_PUBLIC new_chisel(sinsp* inspector, const std::string& name, bool is_file = true);

#endif  // _WRAPPER_H_
