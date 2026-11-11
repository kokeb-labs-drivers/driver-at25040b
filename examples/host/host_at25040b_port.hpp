/**
 * @file host_at25040b_port.hpp
 * @brief The host port: the driver's bus hooks over a simulated AT25040B, so the example runs on
 * any PC.
 *
 * `at25040b_test::MockPortBase` (tests/mock_at25040b_bus.hpp) supplies the required bus hooks over
 * the protocol model the host tests use. This class adds what a real port adds: `DelayUs()` (here
 * it advances the simulated chip's clock instead of waiting), `GpioSet()` for the WP pin and
 * `Log()` to the console. A port for a real bus on a PC (Linux spidev, a USB bridge) implements
 * the same hooks: see docs/porting.md.
 */
#ifndef AT25040B_HOST_AT25040B_PORT_HPP
#define AT25040B_HOST_AT25040B_PORT_HPP

#include <cstdint>
#include <cstdio>

#include "at25040b.hpp"
#include "mock_at25040b_bus.hpp"

/// The simulated board's supply and bus settings: what a real port would program.
inline constexpr uint32_t kHostSupplyMv = 3300;
/// SCK of the simulated bus.
inline constexpr uint32_t kHostClockHz = 1'000'000;
/// SPI mode of the simulated bus.
inline constexpr uint8_t kHostSpiMode = 0;
// The driver's bus requirements (handbook KB-CPP-123), checked at compile time.
static_assert(kHostClockHz <= at25040b::BusTimingAt(kHostSupplyMv).max_clock_hz,
              "SCK above the AT25040B's f_SCK max at this supply");
static_assert(kHostSpiMode == at25040b::kSpiMode || kHostSpiMode == at25040b::kSpiModeAlternate,
              "the AT25040B samples in SPI mode 0 or 3");
/// Bit order of the simulated bus.
inline constexpr bool kHostMsbFirst = true;
static_assert(kHostMsbFirst == at25040b::kMsbFirst, "the AT25040B takes MSB first");

/// The host port: simulated chip, real console.
class HostAt25040bPort : public at25040b_test::MockPortBase<HostAt25040bPort> {
public:
  /// Time passes in the simulated chip; nothing sleeps.
  void DelayUs(uint32_t us) noexcept {
    state.chip.Elapse(us);
  }

  /// The simulated WP pin: INACTIVE is high, writes allowed.
  bool GpioSet(at25040b::CtrlPin pin, at25040b::GpioSignal signal) noexcept {
    if (pin != at25040b::CtrlPin::WriteProtect) {
      return false;
    }
    state.chip.wp_high = signal == at25040b::GpioSignal::INACTIVE;
    return true;
  }

  /// Driver messages (they carry their own `at25040b:` prefix) go to the console, one line each.
  void Log(at25040b::LogLevel level, const char* message) noexcept {
    (void)level;
    std::puts(message); // plain text: never pass it as a format string
  }
};

#endif // AT25040B_HOST_AT25040B_PORT_HPP
