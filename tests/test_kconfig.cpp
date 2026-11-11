/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): the defaults without Kconfig, and menuconfig values when they are set.
 *
 * Built twice: without CONFIG_ macros (KCONFIG_SET undefined) and with the values an sdkconfig.h
 * would define (KCONFIG_SET=1, see tests/CMakeLists.txt).
 */
#include "at25040b_kconfig.hpp"
#include "test_harness.hpp"

#ifdef KCONFIG_SET
TEST_CASE("Kconfig values reach the Config") {
  constexpr at25040b::Config kC = at25040b::KconfigDefaults();
  CHECK_EQ(kC.variant, at25040b::Variant::At25020b);
  CHECK(!kC.check_write_enable);
  CHECK_EQ(kC.write_timeout_us, 20000U);
  CHECK_EQ(at25040b::Validate(kC), at25040b::Error::Ok);
  at25040b::Config (*volatile fn)() noexcept = &at25040b::KconfigDefaults;
  const at25040b::Config run = fn();
  CHECK_EQ(run.variant, at25040b::Variant::At25020b);
  CHECK(!run.check_write_enable);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}") {
  constexpr at25040b::Config kC = at25040b::KconfigDefaults();
  constexpr at25040b::Config kD{};
  CHECK_EQ(kC.variant, kD.variant);
  CHECK_EQ(kC.check_write_enable, kD.check_write_enable);
  CHECK_EQ(kC.write_timeout_us, kD.write_timeout_us);
  // And at run time, through a pointer the compiler cannot fold.
  at25040b::Config (*volatile fn)() noexcept = &at25040b::KconfigDefaults;
  const at25040b::Config run = fn();
  CHECK_EQ(run.write_timeout_us, kD.write_timeout_us);
}
#endif
