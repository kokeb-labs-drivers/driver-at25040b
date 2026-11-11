/**
 * @file ports.cpp
 * @brief The port with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstddef>
#include <cstdint>

#include "at25040b.hpp"

bool platform_spi_init() noexcept;
bool platform_spi_transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept;
void platform_delay_us(uint32_t us) noexcept;
bool platform_gpio_write(int pin, bool high) noexcept;
void platform_log(const char* line) noexcept;

// --8<-- [start:full-port]
class FullPort : public at25040b::SpiInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    return platform_spi_init();
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return platform_spi_transfer(tx, rx, len);
  }

  // Required: write cycles and status polls.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

  // Optional: WP on a GPIO. ACTIVE means "protected", which is a low level on the pin. The driver
  // releases it only around writes and the identity check, and parks it after.
  bool GpioSet(at25040b::CtrlPin pin, at25040b::GpioSignal signal) noexcept {
    if (pin != at25040b::CtrlPin::WriteProtect) {
      return false;
    }
    return platform_gpio_write(kWpGpio, signal == at25040b::GpioSignal::INACTIVE);
  }

  // Optional: one line per lifecycle event or failure. The driver never formats; the port does.
  void Log(at25040b::LogLevel level, const char* message) noexcept {
    if (level > at25040b::LogLevel::Warning) {
      return;
    }
    platform_log(message); // plain text: never use it as a format string
  }

private:
  static constexpr int kWpGpio = 10;
};
// --8<-- [end:full-port]

// --8<-- [start:wp-check]
at25040b::Error CheckWpWiring(at25040b::AT25040B<FullPort>& eeprom) {
  // With the hook, EnsureInitialized() raises WP for WREN: a WP line that does not reach the chip
  // fails the start with DeviceIdMismatch, before any write is attempted.
  return eeprom.EnsureInitialized().error;
}
// --8<-- [end:wp-check]
