#include "sinsp.h"
#include "chisel.h"

sinsp* SINSP_PUBLIC new_inspector() {
  return new sinsp;
}

sinsp_chisel* SINSP_PUBLIC new_chisel(sinsp* inspector, const std::string& name, bool is_file) {
  return new sinsp_chisel(inspector, name, is_file);
}
