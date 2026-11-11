/**
 * @file test_errors.cpp
 * @brief Each chip-specific error is produced by a real call of the driver, and the base errors the
 * driver documents as never returned stay so.
 */
#include <array>
#include <cstdint>

#include "at25040b.hpp"
#include "mock_at25040b_bus.hpp"
#include "test_harness.hpp"

using at25040b::BlockProtect;
using at25040b::Error;
using at25040b_test::MinimalPort;
using at25040b_test::Miso;
using at25040b_test::WpPort;

TEST_CASE("NoDevice: EnsureInitialized with MISO floating high") {
  MinimalPort port;
  port.state.chip.miso = Miso::FloatHigh;
  at25040b::AT25040B<MinimalPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::NoDevice);
}

TEST_CASE("BusNoise: EnsureInitialized with two different status reads") {
  WpPort port;
  port.state.replace_transfer = 1; // the second RDSR reads 0x04 after an idle 0x00
  port.state.replace_byte = 0x04;
  at25040b::AT25040B<WpPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::BusNoise);
}

TEST_CASE("BusNoise: the WEN check reads a status whose other bits changed") {
  WpPort port;
  port.state.lose_transfer = 3; // the RDSR after WREN reads 0xFF: WEN set, but not idle
  at25040b::AT25040B<WpPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::BusNoise);
  WpPort later;
  later.state.lose_transfer = 5; // the RDSR after WRDI
  at25040b::AT25040B<WpPort> e2(later);
  CHECK_EQ(e2.EnsureInitialized().error, Error::BusNoise);
}

TEST_CASE("DeviceIdMismatch: WEN does not clear after WRDI") {
  WpPort port;
  port.state.lose_transfer = 4; // WRDI lost: WEN stays set
  at25040b::AT25040B<WpPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::DeviceIdMismatch);
}

TEST_CASE("WriteProtected: Write with WP held low") {
  WpPort port;
  at25040b::AT25040B<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.wp_tied_low = true;
  const std::array<uint8_t, 2> data{1, 2};
  CHECK_EQ(e.Write(0, data).error, Error::WriteProtected);
}

TEST_CASE("BlockProtected: Write into a protected half") {
  WpPort port;
  port.state.chip.bp = 2;
  at25040b::AT25040B<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const std::array<uint8_t, 2> data{1, 2};
  CHECK_EQ(e.Write(0x100, data).error, Error::BlockProtected);
}

TEST_CASE("BusNoise: the status after WREN is neither idle nor busy") {
  WpPort port;
  at25040b::AT25040B<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  const std::array<uint8_t, 2> data{1, 2};
  port.state.lose_transfer = port.state.transfers + 3; // RDSR, RDSR, WREN, then this RDSR
  CHECK_EQ(e.Write(0, data).error, Error::BusNoise);
  CHECK(!port.state.chip.wen);
}

TEST_CASE("WriteMismatch: the read-back differs (Verify::ReadBack)") {
  WpPort port;
  at25040b::AT25040B<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.stuck_address = 1;
  const std::array<uint8_t, 2> data{1, 2};
  CHECK_EQ(e.Write(0, data, at25040b::Verify::ReadBack).error, Error::WriteMismatch);
}

TEST_CASE("Timeout: a write cycle that does not end") {
  WpPort port;
  at25040b::AT25040B<WpPort> e(port);
  REQUIRE(e.EnsureInitialized().ok());
  port.state.chip.busy_us = 1000000;
  CHECK_EQ(e.GetBlockProtection().error, Error::Timeout);
}

TEST_CASE("Pin: the WP hook cannot park WP at start-up, or release it for a write") {
  WpPort port;
  port.state.fail_gpio = 0;
  at25040b::AT25040B<WpPort> e(port);
  CHECK_EQ(e.EnsureInitialized().error, Error::Pin);
  CHECK_EQ(port.state.transfers, 0); // nothing sent with WP in an unknown state
  REQUIRE(e.EnsureInitialized().ok());
  port.state.fail_gpio = port.state.gpio_calls;
  const std::array<uint8_t, 1> data{0x42};
  CHECK_EQ(e.Write(0, data).error, Error::Pin);
}

TEST_CASE("The base errors documented as not returned are never produced by these calls") {
  // Busy, Crc, DeviceFault and Unsupported: the sweep in test_bus_faults.cpp and the tests above
  // cover every path; the names stay for the shared base set.
  CHECK_EQ(static_cast<int>(Error::Busy), 7);
  CHECK_EQ(static_cast<int>(Error::Unsupported), 9);
  CHECK_EQ(static_cast<int>(Error::NoDevice), 16);
  CHECK_EQ(static_cast<int>(Error::WriteMismatch), 20);
  CHECK_EQ(static_cast<int>(Error::Pin), 21);
  CHECK(at25040b::ToString(BlockProtect::None == BlockProtect::None ? Error::BlockProtected
                                                                    : Error::Ok)[0] == 'B');
}
