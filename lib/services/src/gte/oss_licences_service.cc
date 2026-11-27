#include "gte/oss_licences_service.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <_newlib_version.h>
#include <cmsis_version.h>

#include <cstdio>
#include <cstring>

#include "generated/version.h"

namespace gte {

namespace {

constexpr char kSource[] =
    "https://github.com/M0WUT/p0004-sw-002_claude-firmware";
constexpr char kMit[] = "MIT (https://spdx.org/licenses/MIT.html)";
constexpr char kLgpl[] =
    "LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later.html)";
constexpr char kBsd3[] =
    "BSD-3-Clause (https://spdx.org/licenses/BSD-3-Clause.html)";
constexpr char kApache[] =
    "Apache-2.0 (https://spdx.org/licenses/Apache-2.0.html)";
// newlib has no single SPDX id: COPYING.NEWLIB lists its many licences.
constexpr char kNewlib[] =
    "LicenseRef-newlib (https://sourceware.org/git/"
    "?p=newlib-cygwin.git;a=blob_plain;f=COPYING.NEWLIB)";
constexpr char kGcc[] =
    "GPL-3.0-or-later WITH GCC-exception-3.1 "
    "(https://spdx.org/licenses/GPL-3.0-or-later.html "
    "https://spdx.org/licenses/GCC-exception-3.1.html)";
// Dave Nadler's newlib malloc wrapper in STM32duino FreeRTOS: its own
// BSD-style licence with extra conditions, written in the file itself.
constexpr char kNadlerHeap[] =
    "LicenseRef-Nadler-heap (https://github.com/stm32duino/STM32FreeRTOS/"
    "blob/10.3.2/portable/MemMang/heap_useNewlib_ST.c)";

// "<prefix> <major>.<minor>.<patch>" from a version packed as
// major << 24 | minor << 16 | patch << 8.
void FormatVersion(char *buf, size_t size, const char *prefix,
                   uint32_t packed) {
  snprintf(buf, size, "%s %u.%u.%u", prefix,
           static_cast<unsigned>(packed >> 24),
           static_cast<unsigned>((packed >> 16) & 0xFF),
           static_cast<unsigned>((packed >> 8) & 0xFF));
}

}  // namespace

void OssLicencesService::Initialise() {
  FormatVersion(arduino_core_, sizeof(arduino_core_), "STM32duino Arduino core",
                STM32_CORE_VERSION);
  // Wire and the generic variant ship with the core: "(core x.y.z)"
  const char *core = arduino_core_ + strlen("STM32duino Arduino core ");
  snprintf(wire_, sizeof(wire_), "STM32duino Wire library (core %s)", core);
  snprintf(variant_, sizeof(variant_),
           "STM32duino generic H743V variant (core %s)", core);
  FormatVersion(hal_, sizeof(hal_), "STM32H7 HAL driver", HAL_GetHalVersion());
  FormatVersion(cmsis_device_, sizeof(cmsis_device_), "ST CMSIS STM32H7 device",
                __STM32H7xx_CMSIS_DEVICE_VERSION);
  snprintf(cmsis_core_, sizeof(cmsis_core_), "Arm CMSIS-Core %u.%u",
           static_cast<unsigned>(__CM_CMSIS_VERSION_MAIN),
           static_cast<unsigned>(__CM_CMSIS_VERSION_SUB));
  // tskKERNEL_VERSION_NUMBER is "V10.3.1"
  snprintf(freertos_, sizeof(freertos_), "FreeRTOS kernel %s",
           tskKERNEL_VERSION_NUMBER + 1);
  snprintf(newlib_, sizeof(newlib_), "newlib-nano %s", _NEWLIB_VERSION);
  snprintf(gcc_, sizeof(gcc_), "GCC runtime and libstdc++ %d.%d.%d", __GNUC__,
           __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
}

void OssLicencesService::GetStatus(StatusList *list) {
  list->StaticText("firmware", "GTE Core firmware " VERSION_STRING, kMit);
  list->StaticText("source", "Source code", kSource);
  list->StaticText("arduinoCore", arduino_core_, kLgpl);
  list->StaticText("wire", wire_, kLgpl);
  list->StaticText("genericVariant", variant_, kBsd3);
  list->StaticText("stm32h7Hal", hal_, kBsd3);
  list->StaticText("cmsisDevice", cmsis_device_, kApache);
  list->StaticText("cmsisCore", cmsis_core_, kApache);
  list->StaticText("freertos", freertos_, kMit);
  list->StaticText("freertosHeap", "FreeRTOS newlib heap wrapper (Dave Nadler)",
                   kNadlerHeap);
  list->StaticText("newlib", newlib_, kNewlib);
  list->StaticText("gccRuntime", gcc_, kGcc);
  if (app_) app_->Acknowledge(list);
}

}  // namespace gte
