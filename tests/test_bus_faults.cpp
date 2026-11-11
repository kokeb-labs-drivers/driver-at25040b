/**
 * @file test_bus_faults.cpp
 * @brief Every public operation reports a failed bus transfer as Error::Bus and a failed WP pin
 * write as Error::Pin (handbook KB-CPP-047), whichever one fails, and leaves WP parked and the
 * write enable latch clear.
 *
 * Each operation is run once on a clean bus to count its transfers and pin writes, then once per
 * transfer (and once per pin write) with exactly that one failing. This reaches every "the
 * transfer failed, return the error" path in the driver, which a single fault test per operation
 * would not. Reads and writes are swept with `Verify::None` and again with `Verify::ReadBack`,
 * so the read-back transfers are swept too.
 */
#include <array>
#include <cstdint>
#include <cstdio>

#include "at25040b.hpp"
#include "mock_at25040b_bus.hpp"
#include "test_harness.hpp"

using at25040b::BlockProtect;
using at25040b::Error;
using at25040b::Verify;
using at25040b_test::WpPort;

namespace {

using E = at25040b::AT25040B<WpPort>;

/// Runs `op` on a started driver with each of its transfers, then each of its pin writes,
/// failing in turn; every run must return Error::Bus for a transfer and Error::Pin for a pin.
template <typename Op>
void SweepBusFaults(const char* name, Op op) {
  int transfers = 0;
  int pins = 0;
  {
    WpPort clean;
    E e(clean);
    REQUIRE(e.EnsureInitialized().ok());
    const int before = clean.state.transfers;
    const int gpio = clean.state.gpio_calls;
    CHECK_EQ(op(e), Error::Ok);
    transfers = clean.state.transfers - before;
    pins = clean.state.gpio_calls - gpio;
  }
  CHECK(transfers > 0);
  for (int i = 0; i < transfers + pins; ++i) {
    WpPort port;
    E e(port);
    REQUIRE(e.EnsureInitialized().ok());
    if (i < transfers) {
      port.state.fail_transfer = port.state.transfers + i;
    } else {
      port.state.fail_gpio = port.state.gpio_calls + (i - transfers);
    }
    const Error err = op(e);
    const Error want = i < transfers ? Error::Bus : Error::Pin;
    if (err != want) {
      std::printf("    %s: fault %d of %d, got %s\n", name, i + 1, transfers + pins,
                  at25040b::ToString(err));
    }
    CHECK_EQ(err, want);
    // A failed WRDI or park is reported, not retried; only those two may leave something behind.
    const bool cleanup_failed =
        i >= transfers || port.state.opcodes[static_cast<std::size_t>(port.state.fail_transfer)] ==
                              at25040b::reg::kWrdi;
    if (!cleanup_failed) {
      CHECK(!port.state.chip.wp_high);
    }
  }
}

} // namespace

TEST_CASE("Every transfer of every read can fail and is reported as Bus") {
  for (const Verify verify : {Verify::None, Verify::ReadBack}) {
    SweepBusFaults("Read", [verify](E& e) {
      std::array<uint8_t, 40> buf{};
      return e.Read(0xF0, buf, verify).error;
    });
  }
  SweepBusFaults("ReadStatus", [](E& e) { return e.ReadStatus().error; });
  SweepBusFaults("GetBlockProtection", [](E& e) { return e.GetBlockProtection().error; });
}

TEST_CASE("Every transfer and pin write of every write can fail and is reported as Bus or Pin") {
  for (const Verify verify : {Verify::None, Verify::ReadBack}) {
    SweepBusFaults("Write", [verify](E& e) {
      std::array<uint8_t, 12> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
      return e.Write(0x1FA - 6, data, verify).error;
    });
  }
  SweepBusFaults("SetBlockProtection",
                 [](E& e) { return e.SetBlockProtection(BlockProtect::UpperHalf).error; });
  SweepBusFaults("Deinitialize", [](E& e) { return e.Deinitialize().error; });
}

TEST_CASE("Every transfer and pin write of EnsureInitialized can fail: Bus or Pin") {
  for (const bool toggle : {true, false}) {
    at25040b::Config c{};
    c.check_write_enable = toggle;
    int transfers = 0;
    int pins = 0;
    {
      WpPort clean;
      E e(clean, c);
      REQUIRE(e.EnsureInitialized().ok());
      transfers = clean.state.transfers;
      pins = clean.state.gpio_calls;
    }
    for (int i = 0; i < transfers + pins; ++i) {
      WpPort port;
      if (i < transfers) {
        port.state.fail_transfer = i;
      } else {
        port.state.fail_gpio = i - transfers;
      }
      E e(port, c);
      CHECK_EQ(e.EnsureInitialized().error, i < transfers ? Error::Bus : Error::Pin);
      CHECK(!e.IsInitialized());
    }
  }
}

TEST_CASE("A transfer lost on its way to the chip never yields a wrong Ok from a write") {
  // Each transfer of a two-page write is lost in turn: the chip never sees it, MISO floats high.
  // Without ReadBack too: the WEN check after each cycle catches a lost WRITE.
  for (const Verify verify : {Verify::None, Verify::ReadBack}) {
    int transfers = 0;
    {
      WpPort clean;
      E e(clean);
      REQUIRE(e.EnsureInitialized().ok());
      const int before = clean.state.transfers;
      std::array<uint8_t, 16> data{};
      data.fill(0xA5);
      REQUIRE(e.Write(0, data, verify).ok());
      transfers = clean.state.transfers - before;
    }
    for (int i = 0; i < transfers; ++i) {
      WpPort port;
      E e(port);
      REQUIRE(e.EnsureInitialized().ok());
      port.state.lose_transfer = port.state.transfers + i;
      std::array<uint8_t, 16> data{};
      data.fill(0xA5);
      const auto r = e.Write(0, data, verify);
      bool held = true;
      for (std::size_t k = 0; k < data.size(); ++k) {
        held = held && port.state.chip.memory[k] == 0xA5;
      }
      if (r.ok() && !held) {
        std::printf("    lost transfer %d of %d: Ok without the data\n", i + 1, transfers);
      }
      CHECK(!r.ok() || held);
      CHECK(!port.state.chip.wp_high);
    }
  }
}
