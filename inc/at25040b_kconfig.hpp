/**
 * @file at25040b_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_AT25040B_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-at25040b authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Include this header only where you want menuconfig to choose the defaults; the driver never
 * includes it and no driver signature depends on it. Under ESP-IDF the values come from
 * `sdkconfig.h`. Without Kconfig every option falls back to `Config{}`.
 *
 * Kconfig writes a bool option set to "n" as an absent macro. The identity check is on by default,
 * so its option is a "skip" option: an absent `CONFIG_AT25040B_SKIP_WRITE_ENABLE_CHECK` means the
 * check runs, under ESP-IDF and outside it alike. Read and write verification is chosen per call
 * (`Verify`), not here.
 *
 * One configuration per program: the inline function below depends on the `CONFIG_` macros, so
 * every translation unit of one program must see the same values (ESP-IDF has one `sdkconfig.h`
 * per build; each host test is its own program). No driver header includes this one (handbook
 * KB-CPP20-018).
 */
#ifndef AT25040B_KCONFIG_HPP
#define AT25040B_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "at25040b_types.hpp"

#ifndef CONFIG_AT25040B_WRITE_TIMEOUT_US
/** Write cycle timeout in microseconds when Kconfig does not set it: `Config{}.write_timeout_us`.
 */
#define CONFIG_AT25040B_WRITE_TIMEOUT_US 10000
#endif

namespace at25040b {

/**
 * @brief The configuration menuconfig selected.
 * @return `Config{}` with the variant, the identity check and the write timeout taken from Kconfig.
 */
constexpr Config KconfigDefaults() noexcept {
  Config c{};
#ifdef CONFIG_AT25040B_VARIANT_AT25010B
  c.variant = Variant::At25010b;
#elif defined(CONFIG_AT25040B_VARIANT_AT25020B)
  c.variant = Variant::At25020b;
#else
  c.variant = Variant::At25040b;
#endif
#if defined(CONFIG_AT25040B_SKIP_WRITE_ENABLE_CHECK) && CONFIG_AT25040B_SKIP_WRITE_ENABLE_CHECK
  c.check_write_enable = false;
#endif
  c.write_timeout_us = CONFIG_AT25040B_WRITE_TIMEOUT_US;
  return c;
}

} // namespace at25040b

#endif // AT25040B_KCONFIG_HPP
