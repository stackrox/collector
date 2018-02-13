#include "sinsp.h"
#include "chisel.h"

sinsp* SINSP_PUBLIC new_inspector() {
  return new sinsp;
}

sinsp_chisel* SINSP_PUBLIC new_chisel(sinsp* inspector, const char* name) {
  return new sinsp_chisel(inspector, name);
}
