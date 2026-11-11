/**
 * @file my_port.hpp
 * @brief The SPI port shown on the Quickstart and Porting pages, compiled by the host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces
 * them with its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux spidev).
 */
#ifndef AT25040B_MY_PORT_HPP
#define AT25040B_MY_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "at25040b_spi_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the port compiles.
bool platform_spi_init() noexcept; // mode 0, kMyClockHz, CS high >= 100 ns between transfers
bool platform_spi_transfer(const uint8_t* tx, uint8_t* rx,
                           std::size_t len) noexcept; // CS low for all bytes, high after
void platform_delay_us(uint32_t us) noexcept;         // busy-wait or sleep

// --8<-- [start:port]
// Your board's bus settings, checked against what the chip needs (handbook KB-CPP-123).
inline constexpr uint32_t kMySupplyMv = 3300;     // lowest VCC the board sees
inline constexpr uint32_t kMyClockHz = 8'000'000; // what platform_spi_init() programs
inline constexpr uint8_t kMySpiMode = 0;          // CPOL 0, CPHA 0
inline constexpr bool kMyMsbFirst = true;         // the SDK's bit order
inline constexpr uint32_t kMyCsGapNs = 1000;      // CS high between two transfers, as measured
static_assert(kMyClockHz <= at25040b::BusTimingAt(kMySupplyMv).max_clock_hz,
              "SCK above f_SCK max at this supply (Table 4-3)");
static_assert(kMySpiMode == at25040b::kSpiMode || kMySpiMode == at25040b::kSpiModeAlternate,
              "the chip samples in SPI mode 0 or 3");
static_assert(kMyMsbFirst == at25040b::kMsbFirst, "the chip takes MSB first");
static_assert(kMyCsGapNs >= at25040b::BusTimingAt(kMySupplyMv).min_cs_high_ns,
              "CS high time below t_CS (Table 4-3)");

class MyPort : public at25040b::SpiInterface<MyPort> {
public:
  // Bring the bus up once; later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_spi_init();
    }
    return ready_;
  }

  // One transfer, CS held low for all `len` bytes. `rx` is nullptr when no reply is needed.
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return platform_spi_transfer(tx, rx, len);
  }

  // Wait at least `us` microseconds: the driver polls the status every 100 us during a write.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:port]

#endif // AT25040B_MY_PORT_HPP
