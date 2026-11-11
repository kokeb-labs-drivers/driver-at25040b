/**
 * @file test_at25040b.cpp
 * @brief Behaviour of every public call against the protocol model: lifecycle, identity check,
 * reads, writes, block protection, the WP pin and the bus faults a board can produce.
 */
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "at25040b.hpp"
#include "mock_at25040b_bus.hpp"
#include "test_harness.hpp"

using at25040b::BlockProtect;
using at25040b::Config;
using at25040b::Error;
using at25040b::Variant;
using at25040b::Verify;
using at25040b_test::FullPort;
using at25040b_test::MinimalPort;
using at25040b_test::Miso;
using at25040b_test::WpPort;
namespace reg = at25040b::reg;

namespace {

template <typename Port>
using Eeprom = at25040b::AT25040B<Port>;

template <std::size_t N>
std::array<uint8_t, N> Pattern(uint8_t seed) {
  std::array<uint8_t, N> a{};
  for (std::size_t i = 0; i < N; ++i) {
    a[i] = static_cast<uint8_t>(seed + (i * 7U));
  }
  return a;
}

} // namespace

// --- Lifecycle ----------------------------------------------------------------------------------

TEST_CASE("The constructor does no I/O and every call before the start returns NotInitialized") {
  WpPort port;
  Eeprom<WpPort> e(port);
  std::array<uint8_t, 4> buf{1, 2, 3, 4};
  CHECK(!e.IsInitialized());
  CHECK_EQ(e.Read(0, buf).error, Error::NotInitialized);
  CHECK_EQ(e.Write(0, buf).error, Error::NotInitialized);
  CHECK_EQ(e.ReadStatus().error, Error::NotInitialized);
  CHECK_EQ(e.GetBlockProtection().error, Error::NotInitialized);
  CHECK_EQ(e.SetBlockProtection(BlockProtect::All).error, Error::NotInitialized);
  CHECK_EQ(buf[0], 1); // untouched
  CHECK_EQ(port.state.transfers, 0);
  CHECK_EQ(port.state.gpio_calls, 0);
  CHECK_EQ(port.state.bus_up_calls, 0);
}

TEST_CASE("EnsureInitialized runs the identity check once, then returns Ok with no I/O") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  CHECK(e.IsInitialized());
  const int transfers = port.state.transfers;
  // RDSR, RDSR, WREN, RDSR, WRDI, RDSR
  CHECK_EQ(transfers, 6);
  CHECK_EQ(port.state.CountOpcode(reg::kWren), 1);
  CHECK_EQ(port.state.CountOpcode(reg::kWrdi), 1);
  CHECK_EQ(port.state.CountOpcode(reg::kWrite), 0);
  CHECK_EQ(port.state.CountOpcode(reg::kWrsr), 0);
  CHECK(!port.state.chip.wen);
  CHECK(!port.state.chip.wp_high); // parked
  CHECK_EQ(port.wp_releases, 1);
  CHECK(e.EnsureInitialized().ok());
  CHECK_EQ(port.state.transfers, transfers);
  CHECK_EQ(port.state.bus_up_calls, 1);
}

TEST_CASE("A failed start leaves the driver uninitialised and the next call starts again") {
  MinimalPort port;
  port.state.chip.miso = Miso::FloatHigh;
  Eeprom<MinimalPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::NoDevice);
  CHECK(!e.IsInitialized());
  std::array<uint8_t, 2> buf{};
  CHECK_EQ(e.Read(0, buf).error, Error::NotInitialized);
  port.state.chip.miso = Miso::Chip; // the module is plugged in
  CHECK(e.EnsureInitialized().ok());
  CHECK_EQ(port.state.bus_up_calls, 2);
}

TEST_CASE("An invalid Config fails the start with no I/O") {
  MinimalPort port;
  Config c{};
  c.write_timeout_us = 1000; // below t_WC
  Eeprom<MinimalPort> e(port, c);
  CHECK_EQ(e.EnsureInitialized().error, Error::InvalidArgument);
  CHECK_EQ(port.state.transfers, 0);
  CHECK_EQ(port.state.bus_up_calls, 0);
}

TEST_CASE("The port's bus bring-up failing is Bus") {
  MinimalPort port;
  port.state.bus_up_ok = false;
  Eeprom<MinimalPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::Bus);
  CHECK_EQ(port.state.transfers, 0);
}

TEST_CASE("Deinitialize sends WRDI, parks WP and a later start probes again") {
  WpPort port;
  Eeprom<WpPort> e(port);
  CHECK(e.Deinitialize().ok()); // not started: nothing
  CHECK_EQ(port.state.transfers, 0);
  REQUIRE(e.EnsureInitialized().ok());
  const int before = port.state.transfers;
  CHECK(e.Deinitialize().ok());
  CHECK(!e.IsInitialized());
  CHECK_EQ(port.state.CountOpcode(reg::kWrdi, before), 1);
  CHECK(!port.state.chip.wp_high);
  REQUIRE(e.EnsureInitialized().ok());
  CHECK(port.state.transfers > before + 1);
}

TEST_CASE("Deinitialize reports a failed WRDI as Bus, a failed park as Pin, uninitialised anyway") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.fail_transfer = port.state.transfers;
  CHECK_EQ(e.Deinitialize().error, Error::Bus);
  CHECK(!e.IsInitialized());
  REQUIRE(e.EnsureInitialized().ok());
  port.state.fail_gpio = port.state.gpio_calls;
  CHECK_EQ(e.Deinitialize().error, Error::Pin);
  CHECK(!e.IsInitialized());
  REQUIRE(e.EnsureInitialized().ok());
  port.state.fail_transfer = port.state.transfers; // both fail: WP left released is reported
  port.state.fail_gpio = port.state.gpio_calls;
  CHECK_EQ(e.Deinitialize().error, Error::Pin);
}

TEST_CASE("ApplyConfig stores without I/O, before and after the start, and checks the Config") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  Config c{};
  c.variant = Variant::At25010b;
  CHECK(e.ApplyConfig(c).ok());
  CHECK_EQ(e.GetCapacityBytes(), 128U);
  CHECK_EQ(port.state.transfers, 0);
  REQUIRE(e.EnsureInitialized().ok());
  const int before = port.state.transfers;
  c.variant = Variant::At25020b;
  CHECK(e.ApplyConfig(c).ok());
  CHECK_EQ(e.GetCapacityBytes(), 256U);
  Config bad{};
  bad.variant = static_cast<Variant>(7); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
  CHECK_EQ(e.ApplyConfig(bad).error, Error::InvalidArgument);
  CHECK_EQ(e.GetConfig().variant, Variant::At25020b);
  CHECK_EQ(port.state.transfers, before);
}

TEST_CASE("Validate, CapacityBytes and ProtectedFrom follow Table 6-4") {
  CHECK_EQ(at25040b::Validate(Config{}), Error::Ok);
  Config c{};
  c.write_timeout_us = at25040b::kWriteTimeoutMaxUs + 1;
  CHECK_EQ(at25040b::Validate(c), Error::InvalidArgument);
  c.write_timeout_us = at25040b::kWriteCycleMaxUs;
  CHECK_EQ(at25040b::Validate(c), Error::Ok);
  CHECK_EQ(at25040b::CapacityBytes(
               static_cast<Variant>(3)), // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
           0U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25010b, BlockProtect::UpperQuarter), 0x60U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25010b, BlockProtect::UpperHalf), 0x40U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25020b, BlockProtect::UpperQuarter), 0xC0U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25020b, BlockProtect::UpperHalf), 0x80U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25040b, BlockProtect::UpperQuarter), 0x180U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25040b, BlockProtect::UpperHalf), 0x100U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25040b, BlockProtect::All), 0U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25040b, BlockProtect::None), 0x200U);
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25040b,
                                   static_cast<BlockProtect>(
                                       9)), // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
           0U);
}

TEST_CASE("ToString names every error") {
  CHECK(at25040b::ToString(Error::WriteMismatch)[0] == 'W');
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): tests the fallback
  CHECK(at25040b::ToString(static_cast<Error>(99))[0] == 'U');
  const char* expected[] = {
      "Ok",  "NotInitialized", "InvalidArgument", "Bus",        "Timeout", "DeviceIdMismatch",
      "Crc", "Busy",           "DeviceFault",     "Unsupported"};
  for (int i = 0; i < 10; ++i) {
    CHECK(std::string_view(at25040b::ToString(static_cast<Error>(i))) == expected[i]);
  }
  CHECK(std::string_view(at25040b::ToString(static_cast<Error>(12))) ==
        "Unknown"); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
  CHECK(std::string_view(at25040b::ToString(Error::NoDevice)) == "NoDevice");
  CHECK(std::string_view(at25040b::ToString(Error::BusNoise)) == "BusNoise");
  CHECK(std::string_view(at25040b::ToString(Error::WriteProtected)) == "WriteProtected");
  CHECK(std::string_view(at25040b::ToString(Error::BlockProtected)) == "BlockProtected");
  CHECK(std::string_view(at25040b::ToString(Error::WriteMismatch)) == "WriteMismatch");
  CHECK(std::string_view(at25040b::ToString(Error::Pin)) == "Pin");
  // The chip-specific names at run time too, from values the compiler cannot fold.
  const char* chip_specific[] = {"NoDevice",       "BusNoise",      "WriteProtected",
                                 "BlockProtected", "WriteMismatch", "Pin"};
  volatile int first = 16;
  for (int i = 0; i < 6; ++i) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): every value here is one
    CHECK(std::string_view(at25040b::ToString(static_cast<Error>(first + i))) == chip_specific[i]);
  }
}

TEST_CASE("The bus helper refuses a length its buffer cannot hold, with no transfer") {
  MinimalPort port;
  at25040b::detail::EepromBus<MinimalPort> bus(port);
  std::array<uint8_t, at25040b::kReadChunkBytes + 1U> big{};
  const std::span<uint8_t> dynamic{big};
  CHECK_EQ(bus.Read(0, dynamic), Error::InvalidArgument);              // 17 bytes
  CHECK_EQ(bus.Read(0, dynamic.first(0)), Error::InvalidArgument);     // empty
  CHECK_EQ(bus.Read(0x200, dynamic.first(1)), Error::InvalidArgument); // above A8
  const std::span<const uint8_t> data{big};
  CHECK_EQ(bus.Write(0, data.first(9)), Error::InvalidArgument);     // more than a page
  CHECK_EQ(bus.Write(0x0F, data.first(2)), Error::InvalidArgument);  // crosses a page
  CHECK_EQ(bus.Write(0, data.first(0)), Error::InvalidArgument);     // empty
  CHECK_EQ(bus.Write(0x200, data.first(1)), Error::InvalidArgument); // above A8
  CHECK_EQ(port.state.transfers, 0);
  // At the limits it sends one transfer each, never longer than kMaxTransferBytes.
  CHECK_EQ(bus.Read(0x1F0, dynamic.first(at25040b::kReadChunkBytes)), Error::Ok);
  CHECK_EQ(bus.Write(0x0F, data.first(1)), Error::Ok);
  CHECK_EQ(port.state.transfers, 2);
  CHECK_EQ(port.state.chip.max_frame, at25040b::kMaxTransferBytes);
  CHECK_EQ(port.state.bad_transfers, 0);
}

TEST_CASE("ReadRawStatus reads RDSR once, with no wait and no check, and keeps the byte") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  CHECK_EQ(e.ReadRawStatus().error, Error::NotInitialized);
  CHECK_EQ(port.state.transfers, 0);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.bp = 2;
  int before = port.state.transfers;
  auto r = e.ReadRawStatus();
  REQUIRE(r.ok());
  CHECK_EQ(r.value, 0x08U);
  CHECK_EQ(port.state.transfers - before, 1);
  CHECK_EQ(e.GetLastStatusByte(), 0x08U);
  port.state.chip.busy_us = 1000; // a write cycle runs: no wait, the busy byte as it is
  before = port.state.transfers;
  r = e.ReadRawStatus();
  CHECK_EQ(r.value, 0xFFU);
  CHECK_EQ(port.state.transfers - before, 1);
  CHECK_EQ(port.delayed_us, 0U);
  port.state.replace_transfer = port.state.transfers; // a byte that is no status: not refused
  port.state.replace_byte = 0x5A;
  CHECK_EQ(e.ReadRawStatus().value, 0x5AU);
  port.state.fail_transfer = port.state.transfers; // no retry
  before = port.state.transfers;
  CHECK_EQ(e.ReadRawStatus().error, Error::Bus);
  CHECK_EQ(port.state.transfers - before, 1);
}

// --- Identity check and the bus faults a board can produce ----------------------------------

TEST_CASE("MISO floating high (module unplugged) is NoDevice after the write cycle timeout") {
  MinimalPort port;
  port.state.chip.miso = Miso::FloatHigh;
  Eeprom<MinimalPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::NoDevice);
  CHECK_EQ(e.GetLastStatusByte(), 0xFF);
  CHECK(port.delayed_us >= Config{}.write_timeout_us);
  CHECK_EQ(port.state.CountOpcode(reg::kWren), 0); // nothing but RDSR was sent
}

TEST_CASE("MISO stuck low (an idle-looking 0x00) fails the write enable toggle") {
  WpPort port;
  port.state.chip.miso = Miso::StuckLow;
  Eeprom<WpPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::DeviceIdMismatch);
  CHECK_EQ(e.GetLastStatusByte(), 0x00);
  CHECK(!port.state.chip.wp_high); // parked again
}

TEST_CASE("MISO stuck low passes the fallback check: the documented gap of check_write_enable") {
  MinimalPort port;
  port.state.chip.miso = Miso::StuckLow;
  Config c{};
  c.check_write_enable = false;
  Eeprom<MinimalPort> e(port, c);
  CHECK(e.EnsureInitialized().ok());
  CHECK_EQ(port.state.CountOpcode(reg::kWren), 0);
  CHECK_EQ(port.state.CountOpcode(reg::kRead), 2);
}

TEST_CASE("MOSI echoed on MISO reads status 0x05 and is DeviceIdMismatch") {
  MinimalPort port;
  port.state.chip.miso = Miso::EchoMosi;
  Eeprom<MinimalPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::DeviceIdMismatch);
  CHECK_EQ(e.GetLastStatusByte(), reg::kRdsr);
}

TEST_CASE("Random noise on MISO never passes the identity check") {
  for (uint32_t seed = 1; seed < 200; ++seed) {
    WpPort port;
    port.state.chip.miso = Miso::Noise;
    port.state.chip.noise_state = seed;
    Eeprom<WpPort> e(port);
    const Error err = e.EnsureInitialized().error;
    CHECK(err == Error::DeviceIdMismatch || err == Error::BusNoise || err == Error::NoDevice);
  }
}

TEST_CASE("Random noise passes the fallback check only if eight bytes repeat, which they do not") {
  for (uint32_t seed = 1; seed < 100; ++seed) {
    MinimalPort port;
    port.state.chip.miso = Miso::Noise;
    port.state.chip.noise_state = seed;
    Config c{};
    c.check_write_enable = false;
    Eeprom<MinimalPort> e(port, c);
    CHECK(!e.EnsureInitialized().ok());
  }
}

TEST_CASE("HOLD strapped to CS (section 5) makes the status read 0xFF: NoDevice") {
  MinimalPort port;
  port.state.chip.hold_tied_to_cs = true;
  Eeprom<MinimalPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::NoDevice);
}

TEST_CASE("WP tied low fails the toggle; check_write_enable = false starts it read-only") {
  MinimalPort port;
  port.state.chip.wp_tied_low = true;
  {
    Eeprom<MinimalPort> e(port);
    CHECK_EQ(e.EnsureInitialized().error, Error::DeviceIdMismatch);
  }
  Config c{};
  c.check_write_enable = false;
  Eeprom<MinimalPort> e(port, c);
  REQUIRE(e.EnsureInitialized().ok());
  std::array<uint8_t, 4> buf{};
  CHECK(e.Read(0, buf).ok());
  const auto data = Pattern<4>(1);
  CHECK_EQ(e.Write(0, data).error, Error::WriteProtected);
  CHECK_EQ(port.state.chip.write_cycles, 0);
}

TEST_CASE("A write cycle left running by an MCU reset is waited for at start-up") {
  WpPort port;
  port.state.chip.busy_us = 4000; // a cycle the previous firmware started
  Eeprom<WpPort> e(port);
  CHECK(e.EnsureInitialized().ok());
  CHECK(port.delayed_us >= 4000);
}

TEST_CASE("The latch left set by an earlier WREN does not fail the identity check") {
  WpPort port;
  port.state.chip.wen = true;
  Eeprom<WpPort> e(port);
  CHECK(e.EnsureInitialized().ok());
  CHECK(!port.state.chip.wen);
}

TEST_CASE("A latch left set with WP held low still fails the toggle (fuzzer finding)") {
  // Found by the fuzzer (tests/fuzz/corpus/fuzz_at25040b/regress-wen-preset-wp-low): with WEN
  // already 1, "WREN then WEN = 1" passed although WP blocked the WREN. The driver now clears the
  // latch first.
  MinimalPort port;
  port.state.chip.wen = true;
  port.state.chip.wp_tied_low = true;
  Eeprom<MinimalPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::DeviceIdMismatch);
  CHECK(!port.state.chip.wen);
}

TEST_CASE("A latch left set that WRDI cannot clear fails the start") {
  WpPort port;
  port.state.chip.wen = true;
  port.state.lose_transfer = 2; // RDSR, RDSR, then the first WRDI never reaches the chip
  Eeprom<WpPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::DeviceIdMismatch);
}

TEST_CASE("With the whole array protected every write is refused") {
  WpPort port;
  port.state.chip.bp = 3;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<1>(1);
  CHECK_EQ(e.Write(0, data).error, Error::BlockProtected);
  volatile uint8_t bad = 5; // a level read from somewhere that is not one
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): tests the fallback
  CHECK_EQ(at25040b::ProtectedFrom(Variant::At25040b, static_cast<BlockProtect>(bad)), 0U);
}

TEST_CASE("A block-protected part passes the identity check with its BP bits") {
  WpPort port;
  port.state.chip.bp = 3;
  Eeprom<WpPort> e(port);
  CHECK(e.EnsureInitialized().ok());
  const auto level = e.GetBlockProtection();
  REQUIRE(level.ok());
  CHECK_EQ(level.value, BlockProtect::All);
}

// --- Reads --------------------------------------------------------------------------------------

TEST_CASE("Read returns the array, in chunks of kReadChunkBytes, A8 set above 0xFF") {
  MinimalPort port;
  for (std::size_t i = 0; i < 512; ++i) {
    port.state.chip.memory[i] = static_cast<uint8_t>(i ^ (i >> 8U));
  }
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  std::array<uint8_t, 512> image{};
  const int before = port.state.transfers;
  REQUIRE(e.Read(0, image).ok());
  for (std::size_t i = 0; i < 512; ++i) {
    CHECK_EQ(image[i], static_cast<uint8_t>(i ^ (i >> 8U)));
  }
  CHECK(port.state.chip.max_frame <= at25040b::kMaxTransferBytes);
  // Defaults: two RDSR (idle, confirmed), then each of the 32 chunks once.
  CHECK_EQ(port.state.transfers - before, 2 + 32);
  CHECK_EQ(port.state.CountOpcode(reg::kRead, before) +
               port.state.CountOpcode(reg::kRead | reg::kOpcodeA8, before),
           32);
  CHECK(port.state.CountOpcode(reg::kRead | reg::kOpcodeA8, before) > 0);
}

TEST_CASE("A default Read reads each chunk once") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  std::array<uint8_t, 40> buf{};
  const int before = port.state.transfers;
  REQUIRE(e.Read(0x1F0, std::span{buf}.first(16)).ok());
  CHECK_EQ(port.state.transfers - before, 3);
  REQUIRE(e.Read(3, buf).ok());
  CHECK_EQ(port.state.transfers - before, 3 + 5);
}

TEST_CASE("Read with Verify::ReadBack reads each chunk twice") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  std::array<uint8_t, 40> buf{};
  const int before = port.state.transfers;
  REQUIRE(e.Read(0x1F0, std::span{buf}.first(16), Verify::ReadBack).ok());
  CHECK_EQ(port.state.transfers - before, 2 + 2);
  REQUIRE(e.Read(3, buf, Verify::ReadBack).ok());
  CHECK_EQ(port.state.transfers - before, 4 + 2 + (2 * 3));
}

TEST_CASE("Read refuses an empty or out-of-range request, with no I/O") {
  MinimalPort port;
  Config c{};
  c.variant = Variant::At25020b;
  Eeprom<MinimalPort> e(port, c);
  REQUIRE(e.EnsureInitialized().ok());
  std::array<uint8_t, 4> buf{9, 9, 9, 9};
  const int before = port.state.transfers;
  CHECK_EQ(e.Read(0, std::span<uint8_t>{}).error, Error::InvalidArgument);
  CHECK_EQ(e.Read(0, std::span{buf}.first(0)).error, Error::InvalidArgument);
  CHECK_EQ(e.Read(256, std::span{buf}.first(1)).error, Error::InvalidArgument);
  CHECK_EQ(e.Read(253, buf).error, Error::InvalidArgument); // would wrap
  volatile uint8_t bad = 2; // a Verify read from somewhere that is not one
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): tests the refusal
  CHECK_EQ(e.Read(0, buf, static_cast<Verify>(bad)).error, Error::InvalidArgument);
  CHECK_EQ(buf[0], 9); // untouched
  CHECK(e.Read(252, buf).ok());
  CHECK_EQ(port.state.transfers - before, 3); // RDSR, RDSR, one READ
}

TEST_CASE("One status read that looks idle during a write cycle does not start a read (fuzzer)") {
  // Found by the fuzzer (regress-idle-glitch-during-cycle): a lost RDSR read 0x00 while a cycle
  // ran, the chip ignored the READs, both copies read 0xFF, and Read() returned Ok. An idle status
  // now counts only when the next read confirms it.
  MinimalPort port;
  for (std::size_t i = 0; i < 16; ++i) {
    port.state.chip.memory[i] = static_cast<uint8_t>(i);
  }
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.busy_us = 3000;
  port.state.replace_transfer = port.state.transfers; // the first RDSR reads 0x00
  std::array<uint8_t, 16> buf{};
  REQUIRE(e.Read(0, buf).ok());
  CHECK_EQ(buf[15], 15);
  CHECK(port.delayed_us >= 3000);
}

TEST_CASE("A read during a write cycle waits for it") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.busy_us = 2000;
  std::array<uint8_t, 4> buf{};
  CHECK(e.Read(0, buf).ok());
  CHECK(port.delayed_us >= 2000);
}

TEST_CASE("With Verify::ReadBack, noise on one read of a chunk is BusNoise, the buffer zeroed") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  std::array<uint8_t, 8> buf{};
  // Lose the second read of the chunk: it returns 0xFF where the chip holds 0x00.
  port.state.chip.memory.fill(0x00);
  port.state.lose_transfer = port.state.transfers + 3;
  CHECK_EQ(e.Read(0, buf, Verify::ReadBack).error, Error::BusNoise);
  for (const uint8_t b : buf) {
    CHECK_EQ(b, 0);
  }
}

TEST_CASE("A default Read does not see noise on its one READ: the caller's CRC has to") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  std::array<uint8_t, 8> buf{};
  port.state.chip.memory.fill(0x00);
  port.state.lose_transfer = port.state.transfers + 2; // the only READ: MISO floats high
  CHECK(e.Read(0, buf).ok());
  CHECK_EQ(buf[0], 0xFF);
}

TEST_CASE("The chip unplugged after the start: reads time out, then a re-probe says NoDevice") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.miso = Miso::FloatHigh;
  std::array<uint8_t, 8> buf{};
  CHECK_EQ(e.Read(0, buf).error, Error::Timeout);
  (void)e.Deinitialize();
  CHECK_EQ(e.EnsureInitialized().error, Error::NoDevice);
}

TEST_CASE("After the start a status byte that is neither idle nor busy is BusNoise") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.miso = Miso::EchoMosi;
  std::array<uint8_t, 8> buf{};
  CHECK_EQ(e.Read(0, buf).error, Error::BusNoise);
  CHECK_EQ(e.ReadStatus().error, Error::BusNoise);
  CHECK_EQ(e.GetBlockProtection().error, Error::BusNoise);
}

// --- Writes -------------------------------------------------------------------------------------

TEST_CASE("Write splits at page boundaries, waits each cycle and, with ReadBack, reads back") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<21>(3);
  const int before = port.state.transfers;
  REQUIRE(e.Write(0x0FD, data, Verify::ReadBack).ok());
  for (std::size_t i = 0; i < data.size(); ++i) {
    CHECK_EQ(port.state.chip.memory[0x0FD + i], data[i]);
  }
  CHECK_EQ(port.state.chip.write_cycles, 4); // 0xFD-0xFF, 0x100-0x107, 0x108-0x10F, 0x110-0x111
  CHECK_EQ(port.state.CountOpcode(reg::kWren, before), 4);
  CHECK_EQ(port.state.CountOpcode(reg::kWrite, before), 1);
  CHECK_EQ(port.state.CountOpcode(reg::kWrite | reg::kOpcodeA8, before), 3);
  CHECK_EQ(port.state.CountOpcode(reg::kRead, before), 1); // one read-back per page
  CHECK_EQ(port.state.CountOpcode(reg::kRead | reg::kOpcodeA8, before), 3);
  CHECK(port.state.chip.memory[0x0FC] == 0xFF && port.state.chip.memory[0x112] == 0xFF);
  CHECK(!port.state.chip.wen);
  CHECK(!port.state.chip.wp_high);
  CHECK_EQ(port.wp_releases, 2); // identity check, then this write
  CHECK(port.delayed_us >= uint64_t{4} * port.state.chip.write_cycle_us);
}

TEST_CASE("A whole-array image: 512 bytes from address 0, then read back whole") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const auto image = Pattern<512>(0x5A);
  REQUIRE(e.Write(0, image).ok());
  CHECK_EQ(port.state.chip.write_cycles, 64);
  (void)e.Deinitialize();
  REQUIRE(e.EnsureInitialized().ok()); // probe again after the write
  std::array<uint8_t, 512> back{};
  REQUIRE(e.Read(0, back).ok());
  CHECK(back == image);
}

TEST_CASE("Write refuses an empty or out-of-range request, with no I/O") {
  WpPort port;
  Config c{};
  c.variant = Variant::At25010b;
  Eeprom<WpPort> e(port, c);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<8>(1);
  const int before = port.state.transfers;
  const int gpio = port.state.gpio_calls;
  CHECK_EQ(e.Write(0, std::span<const uint8_t>{}).error, Error::InvalidArgument);
  CHECK_EQ(e.Write(0, std::span{data}.first(0)).error, Error::InvalidArgument);
  CHECK_EQ(e.Write(0x7C, std::span{data}.first(5)).error, Error::InvalidArgument);
  CHECK_EQ(port.state.transfers, before);
  CHECK_EQ(port.state.gpio_calls, gpio);
}

TEST_CASE("A default Write reads no page back") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<16>(1);
  const int before = port.state.transfers;
  REQUIRE(e.Write(0x0F8, data).ok());
  CHECK_EQ(port.state.chip.write_cycles, 2);
  CHECK_EQ(port.state.CountOpcode(reg::kRead, before), 0);
  CHECK_EQ(port.state.CountOpcode(reg::kRead | reg::kOpcodeA8, before), 0);
  // Two RDSR before, then per page WREN, RDSR (WEN = 1), WRITE and the status polls of the cycle,
  // the last two idle (confirmed): nothing else.
  const int rdsr = port.state.CountOpcode(reg::kRdsr, before);
  CHECK_EQ(port.state.transfers - before, rdsr + (2 * 2));
  CHECK(rdsr >= 2 + (2 * 3));
}

TEST_CASE("A write into the protected part is refused before WREN, WP never released") {
  WpPort port;
  port.state.chip.bp = 1; // upper quarter: 0x180 to 0x1FF
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<4>(1);
  const int before = port.state.transfers;
  const int releases = port.wp_releases;
  CHECK_EQ(e.Write(0x17E, data).error, Error::BlockProtected);
  CHECK_EQ(port.state.transfers - before, 2); // the status reads only
  CHECK_EQ(port.wp_releases, releases);
  CHECK(e.Write(0x17C, data).ok()); // ends at 0x17F: allowed
}

TEST_CASE("WP that does not rise (hook ignored on the board) is WriteProtected, WP parked") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.wp_tied_low = true;
  const auto data = Pattern<4>(1);
  CHECK_EQ(e.Write(0, data).error, Error::WriteProtected);
  CHECK_EQ(port.state.chip.write_cycles, 0);
  CHECK(!port.state.chip.wp_high);
  CHECK(!port.state.chip.wen);
}

TEST_CASE("A WRITE the chip silently ignored is WriteMismatch, even without ReadBack") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.ignore_writes = 1;
  const auto data = Pattern<12>(1);
  const int before = port.state.transfers;
  CHECK_EQ(e.Write(0, data).error, Error::WriteMismatch);
  CHECK_EQ(port.state.chip.write_cycles, 0); // the second page was never sent
  CHECK(!port.state.chip.wen);               // WRDI after the failure
  CHECK_EQ(port.state.CountOpcode(reg::kWrdi, before), 1);
  CHECK(!port.state.chip.wp_high);
}

TEST_CASE("A cell that does not take its value is WriteMismatch with Verify::ReadBack") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.stuck_address = 0x12;
  const auto data = Pattern<16>(1);
  CHECK_EQ(e.Write(0x08, data, Verify::ReadBack).error, Error::WriteMismatch);
  CHECK_EQ(port.state.chip.memory[0x08], data[0]); // the first page was written and verified
  CHECK_EQ(port.state.chip.write_cycles, 2);       // nothing after the failing page
}

TEST_CASE("A default Write does not read the stuck cell back; an invalid Verify sends nothing") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.stuck_address = 0x12;
  const auto data = Pattern<16>(1);
  CHECK(e.Write(0x08, data).ok());
  CHECK(port.state.chip.memory[0x12] != data[0x12 - 0x08]);
  const int before = port.state.transfers;
  const int gpio = port.state.gpio_calls;
  volatile uint8_t bad = 7;
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): tests the refusal
  CHECK_EQ(e.Write(0, std::span{data}.first(1), static_cast<Verify>(bad)).error,
           Error::InvalidArgument);
  CHECK_EQ(port.state.transfers, before);
  CHECK_EQ(port.state.gpio_calls, gpio);
}

TEST_CASE("A write cycle that never ends is Timeout, WP parked") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.write_cycle_us = 50000;
  const auto data = Pattern<4>(1);
  CHECK_EQ(e.Write(0, data).error, Error::Timeout);
  CHECK(!port.state.chip.wp_high);
}

TEST_CASE("A port without the WP hook writes when the board ties WP high") {
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<8>(9);
  REQUIRE(e.Write(0x40, data).ok());
  CHECK_EQ(port.state.chip.memory[0x47], data[7]);
}

TEST_CASE("A failing WP hook is Pin: release refused, or park refused after a good write") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<4>(1);
  port.state.fail_gpio = port.state.gpio_calls; // the release
  CHECK_EQ(e.Write(0, data).error, Error::Pin);
  CHECK_EQ(port.state.chip.write_cycles, 0);
  port.state.fail_gpio = port.state.gpio_calls + 1; // the park
  CHECK_EQ(e.Write(0, data).error, Error::Pin);
  CHECK_EQ(port.state.chip.memory[3], data[3]); // written, but WP could not be parked
}

TEST_CASE("A failed step and a failed park together report Pin: WP may be left released") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<4>(1);
  port.state.fail_transfer = port.state.transfers + 2; // the WREN, after the two status reads
  port.state.fail_gpio = port.state.gpio_calls + 1;    // the park
  CHECK_EQ(e.Write(0, data).error, Error::Pin);
  CHECK_EQ(port.state.chip.write_cycles, 0);
}

TEST_CASE("A port that leaves rx unwritten never returns an earlier transfer's bytes") {
  // Regression: with member transfer buffers, a READ whose rx the port did not fill would hand
  // back the bytes of the previous READ (another address), and Verify::ReadBack could not see it.
  MinimalPort port;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.memory[0x000] = 0x11;
  port.state.chip.memory[0x100] = 0x22;
  std::array<uint8_t, 1> first{};
  REQUIRE(e.Read(0x000, first).ok());
  CHECK_EQ(first[0], 0x11U);
  port.state.rx_untouched_from = port.state.transfers + 2; // the READs, after the status reads
  std::array<uint8_t, 1> second{};
  const auto r = e.Read(0x100, second, Verify::ReadBack);
  CHECK(r.ok()); // the bus cannot tell, but the bytes are MISO's idle level, not 0x11
  CHECK_EQ(second[0], 0xFFU);
}

TEST_CASE("Writes on the smaller parts never set A8 and stay in their array") {
  WpPort port;
  port.state.chip.variant = at25040b_test::chip::Part::At25020b;
  Config c{};
  c.variant = Variant::At25020b;
  Eeprom<WpPort> e(port, c);
  REQUIRE(e.EnsureInitialized().ok());
  const auto data = Pattern<8>(2);
  const int before = port.state.transfers;
  REQUIRE(e.Write(0xF8, data).ok());
  CHECK_EQ(port.state.CountOpcode(reg::kWrite | reg::kOpcodeA8, before), 0);
  CHECK_EQ(port.state.chip.memory[0xFF], data[7]);
}

// --- Status and block protection ----------------------------------------------------------------

TEST_CASE("ReadStatus decodes the status and reports a write cycle as busy") {
  MinimalPort port;
  port.state.chip.bp = 2;
  Eeprom<MinimalPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  auto s = e.ReadStatus();
  REQUIRE(s.ok());
  CHECK(!s.value.busy);
  CHECK(!s.value.write_enabled);
  CHECK_EQ(s.value.block_protect, BlockProtect::UpperHalf);
  CHECK_EQ(s.value.raw, 0x08);
  port.state.chip.busy_us = 100;
  s = e.ReadStatus();
  REQUIRE(s.ok());
  CHECK(s.value.busy);
  CHECK_EQ(s.value.raw, 0xFF);
}

TEST_CASE("SetBlockProtection writes BP1:BP0 with one write cycle and reads it back") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  REQUIRE(e.SetBlockProtection(BlockProtect::UpperQuarter).ok());
  CHECK_EQ(port.state.chip.bp, 1);
  CHECK_EQ(port.state.chip.wrsr_cycles, 1);
  CHECK(!port.state.chip.wp_high);
  CHECK(!port.state.chip.wen);
  // Already set: no write cycle.
  REQUIRE(e.SetBlockProtection(BlockProtect::UpperQuarter).ok());
  CHECK_EQ(port.state.chip.wrsr_cycles, 1);
  // Non-volatile: survives a power cycle.
  port.state.chip.PowerCycle();
  const auto level = e.GetBlockProtection();
  REQUIRE(level.ok());
  CHECK_EQ(level.value, BlockProtect::UpperQuarter);
  REQUIRE(e.SetBlockProtection(BlockProtect::None).ok());
  CHECK_EQ(port.state.chip.bp, 0);
}

TEST_CASE("SetBlockProtection refuses a value that is not a level, with no I/O") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const int before = port.state.transfers;
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): tests the argument check
  CHECK_EQ(e.SetBlockProtection(static_cast<BlockProtect>(4)).error, Error::InvalidArgument);
  CHECK_EQ(port.state.transfers, before);
}

TEST_CASE("SetBlockProtection with WP low is WriteProtected; a WRSR that does not take is "
          "WriteMismatch") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.wp_tied_low = true;
  CHECK_EQ(e.SetBlockProtection(BlockProtect::All).error, Error::WriteProtected);
  CHECK_EQ(port.state.chip.bp, 0);
  port.state.chip.wp_tied_low = false;
  // The WRSR frame is lost: no cycle, BP unchanged, WEN still set and cleared by the driver.
  port.state.lose_transfer = port.state.transfers + 4; // RDSR, RDSR, WREN, RDSR, then WRSR
  CHECK_EQ(e.SetBlockProtection(BlockProtect::All).error, Error::WriteMismatch);
  CHECK_EQ(port.state.chip.bp, 0);
  CHECK(!port.state.chip.wen);
  CHECK(!port.state.chip.wp_high);
}

TEST_CASE("A WRSR cycle that never ends is Timeout") {
  WpPort port;
  Eeprom<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.write_cycle_us = 50000;
  CHECK_EQ(e.SetBlockProtection(BlockProtect::UpperHalf).error, Error::Timeout);
  CHECK(!port.state.chip.wp_high);
}

TEST_CASE("The FullPort logs the start and each failure") {
  FullPort port;
  at25040b::AT25040B<FullPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  CHECK_EQ(port.log_lines, 1);
  port.state.chip.miso = Miso::FloatHigh;
  std::array<uint8_t, 2> buf{};
  CHECK_EQ(e.Read(0, buf).error, Error::Timeout);
  CHECK_EQ(port.error_lines, 1);
}

TEST_CASE("The driver version matches the build settings") {
  CHECK(at25040b::GetDriverVersion() != nullptr);
  CHECK_EQ(at25040b::GetDriverVersionMajor(), static_cast<uint32_t>(AT25040B_VERSION_MAJOR));
}
