/**
 * @file mock_at25040b_bus.hpp
 * @brief SPI mock ports over the chip model.
 *
 * `MockPortBase<Self>` implements the required hooks of the SPI port over a `MockBusState`. Derive
 * from it to build a port with any set of optional hooks:
 *
 * | Port | Hooks |
 * | --- | --- |
 * | `MinimalPort` | the required ones plus `DelayUs()`; WP is tied high on this board |
 * | `WpPort` | also `GpioSet()`: the port drives WP |
 * | `FullPort` | also `Log()` |
 */
#ifndef AT25040B_MOCK_AT25040B_BUS_HPP
#define AT25040B_MOCK_AT25040B_BUS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "at25040b_spi_interface.hpp"
#include "at25040b_types.hpp"
#include "mock_at25040b.hpp"

namespace at25040b_test {

/// Shared state of the mock ports: the chip, fault injection and counters.
struct MockBusState {
  At25040bModel chip;
  bool bus_up_ok = true;      ///< EnsureInitialized() result.
  int fail_transfer = -1;     ///< Fail the transfer with this index (0-based); -1 never.
  int lose_transfer = -1;     ///< Report this transfer as done, but the chip never sees it.
  int replace_transfer = -1;  ///< Replace every MISO byte of this transfer with `replace_byte`.
  uint8_t replace_byte = 0;   ///< What MISO carries in `replace_transfer`.
  int rx_untouched_from = -1; ///< From this transfer on, report success but never write `rx`.
  int fail_gpio = -1;         ///< Fail the GpioSet() call with this index; -1 never.
  int transfers = 0;          ///< Transfers so far.
  int gpio_calls = 0;         ///< GpioSet() calls so far.
  int bus_up_calls = 0;       ///< EnsureInitialized() calls so far.
  int bad_transfers = 0;      ///< Transfers outside the port contract (empty, too long, no tx).
  std::array<uint8_t, 2048> opcodes{}; ///< First byte of each transfer, in order.

  bool DoTransfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    const int index = transfers++;
    if (tx == nullptr || len == 0 || len > at25040b::kMaxTransferBytes) {
      bad_transfers++;
      return false;
    }
    if (static_cast<std::size_t>(index) < opcodes.size()) {
      opcodes[static_cast<std::size_t>(index)] = tx[0];
    }
    if (index == fail_transfer) {
      return false;
    }
    if (rx_untouched_from >= 0 && index >= rx_untouched_from) {
      return true; // a port that claims success and leaves the receive buffer as it was
    }
    if (index == lose_transfer) {
      if (rx != nullptr) {
        for (std::size_t i = 0; i < len; ++i) {
          rx[i] = 0xFF; // nothing drove MISO
        }
      }
      return true;
    }
    chip.Frame(tx, rx, len);
    if (index == replace_transfer && rx != nullptr) {
      for (std::size_t i = 0; i < len; ++i) {
        rx[i] = replace_byte;
      }
    }
    return true;
  }

  /// How many of the transfers from `from` on began with `opcode`.
  [[nodiscard]] int CountOpcode(uint8_t opcode, int from = 0) const noexcept {
    int n = 0;
    for (int i = from; i < transfers && static_cast<std::size_t>(i) < opcodes.size(); ++i) {
      n += opcodes[static_cast<std::size_t>(i)] == opcode ? 1 : 0;
    }
    return n;
  }
};

/// The required SPI hooks over a MockBusState, and the delay that runs the chip's clock.
/// Derive `class P : public MockPortBase<P>`.
template <typename Self>
class MockPortBase : public at25040b::SpiInterface<Self> {
public:
  MockBusState state;

  bool EnsureInitialized() noexcept {
    state.bus_up_calls++;
    return state.bus_up_ok;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return state.DoTransfer(tx, rx, len);
  }
};

/// Required hooks plus the delay; time passes in the model as the driver waits.
class MinimalPort : public MockPortBase<MinimalPort> {
public:
  uint64_t delayed_us = 0; ///< Sum of DelayUs() arguments.

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    state.chip.Elapse(us);
  }
};

/// Also drives WP through GpioSet().
class WpPort : public MockPortBase<WpPort> {
public:
  uint64_t delayed_us = 0; ///< Sum of DelayUs() arguments.
  int wp_releases = 0;     ///< GpioSet(WriteProtect, INACTIVE) calls that succeeded.

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    state.chip.Elapse(us);
  }
  bool GpioSet(at25040b::CtrlPin pin, at25040b::GpioSignal signal) noexcept {
    const int index = state.gpio_calls++;
    if (index == state.fail_gpio || pin != at25040b::CtrlPin::WriteProtect) {
      return false;
    }
    state.chip.wp_high = signal == at25040b::GpioSignal::INACTIVE;
    wp_releases += state.chip.wp_high ? 1 : 0;
    return true;
  }
};

/// Every optional hook, with counters.
class FullPort : public MockPortBase<FullPort> {
public:
  uint64_t delayed_us = 0; ///< Sum of DelayUs() arguments.
  int log_lines = 0;       ///< Log() calls.
  int error_lines = 0;     ///< Log() calls at LogLevel::Error.

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    state.chip.Elapse(us);
  }
  bool GpioSet(at25040b::CtrlPin pin, at25040b::GpioSignal signal) noexcept {
    const int index = state.gpio_calls++;
    if (index == state.fail_gpio || pin != at25040b::CtrlPin::WriteProtect) {
      return false;
    }
    state.chip.wp_high = signal == at25040b::GpioSignal::INACTIVE;
    return true;
  }
  void Log(at25040b::LogLevel level, const char* message) noexcept {
    (void)message;
    log_lines++;
    if (level == at25040b::LogLevel::Error) {
      error_lines++;
    }
  }
};

} // namespace at25040b_test

#endif // AT25040B_MOCK_AT25040B_BUS_HPP
