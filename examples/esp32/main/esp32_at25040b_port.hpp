/**
 * @file esp32_at25040b_port.hpp
 * @brief ESP-IDF port for the AT25040B driver: spi_master for the bus, a GPIO for WP, esp_rom
 * delays and ESP_LOG for the log hook.
 *
 * Start from this file for any ESP-IDF target. The pins are in Esp32At25040bPins; the defaults
 * match the wiring table in examples/esp32/README.md (ESP32-C6 DevKitC-1).
 */
#ifndef AT25040B_ESP32_AT25040B_PORT_HPP
#define AT25040B_ESP32_AT25040B_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "at25040b.hpp"

/// The board's supply: the example board powers the EEPROM from 3.3 V.
inline constexpr uint32_t kEsp32At25040bSupplyMv = 3300;
/// The port's default SPI clock: 1 MHz for bring-up on jumper wires.
inline constexpr int kEsp32At25040bClockHz = 1'000'000;
/// The port's SPI mode.
inline constexpr int kEsp32At25040bSpiMode = 0;
/// This port sends MSB first (no SPI_DEVICE_BIT_LSBFIRST).
inline constexpr bool kEsp32At25040bMsbFirst = true;

/// SPI clock cycles CS stays low after the last bit (spi_master `cs_ena_posttrans`, at most 16) so
/// that it lasts at least t_CSH at `clock_hz`: ceil(t_CSH x f / 1e9), at least 1.
constexpr uint32_t Esp32At25040bCsHoldCycles(uint32_t clock_hz, uint32_t cs_hold_ns) noexcept {
  const uint64_t cycles =
      (static_cast<uint64_t>(cs_hold_ns) * clock_hz + 999'999'999U) / 1'000'000'000U;
  return cycles < 1U ? 1U : static_cast<uint32_t>(cycles);
}
/// spi_master cannot stretch CS setup on a full-duplex transfer: CS falls about half an SCK period
/// before the first edge, so the half period must cover t_CSS (not verified on hardware).
constexpr bool Esp32At25040bCsSetupOk(uint32_t clock_hz, uint32_t cs_setup_ns) noexcept {
  return clock_hz > 0U && 500'000'000U / clock_hz >= cs_setup_ns;
}
// The bus requirements of the driver (handbook KB-CPP-123), checked at compile time.
static_assert(static_cast<uint32_t>(kEsp32At25040bClockHz) <=
                  at25040b::BusTimingAt(kEsp32At25040bSupplyMv).max_clock_hz,
              "the default clock is above the AT25040B's f_SCK max at this supply (Table 4-3)");
static_assert(kEsp32At25040bSpiMode == at25040b::kSpiMode ||
                  kEsp32At25040bSpiMode == at25040b::kSpiModeAlternate,
              "the AT25040B samples in SPI mode 0 or 3");
static_assert(kEsp32At25040bMsbFirst == at25040b::kMsbFirst, "the AT25040B takes MSB first");
static_assert(Esp32At25040bCsSetupOk(static_cast<uint32_t>(kEsp32At25040bClockHz),
                                     at25040b::BusTimingAt(kEsp32At25040bSupplyMv).cs_setup_ns),
              "CS setup below t_CSS at the default clock");
static_assert(Esp32At25040bCsHoldCycles(20'000'000U, 100U) == 2U &&
              Esp32At25040bCsHoldCycles(1'000'000U, 200U) == 1U);

/// Pins and bus settings of the ESP-IDF port.
struct Esp32At25040bPins {
  spi_host_device_t host = SPI2_HOST;
  int sclk = 6;
  int mosi = 7;
  int miso = 2;
  int cs = 10;
  /// WP on a GPIO. With -1 the GpioSet() hook fails, so the driver's start fails with `Pin`: for a
  /// board with WP tied, use a port without GpioSet() instead.
  int wp = 11;
  /// Up to `at25040b::BusTimingAt(supply_mv).max_clock_hz`, and half an SCK period must cover
  /// t_CSS: 5 MHz at 3.3 V with this port. Checked at start.
  int clock_hz = kEsp32At25040bClockHz;
  int spi_mode = kEsp32At25040bSpiMode;        ///< Mode 0 or 3; checked at start.
  uint32_t supply_mv = kEsp32At25040bSupplyMv; ///< The EEPROM's VCC, for the clock limit.
};

/// The AT25040B SPI port on ESP-IDF, with every optional hook.
class Esp32At25040bPort : public at25040b::SpiInterface<Esp32At25040bPort> {
public:
  explicit Esp32At25040bPort(const Esp32At25040bPins& pins = {}) noexcept : pins_(pins) {}

  bool EnsureInitialized() noexcept {
    if (device_ != nullptr) {
      return true;
    }
    const at25040b::BusTiming timing = at25040b::BusTimingAt(pins_.supply_mv);
    if (pins_.clock_hz <= 0 || static_cast<uint32_t>(pins_.clock_hz) > timing.max_clock_hz ||
        !Esp32At25040bCsSetupOk(static_cast<uint32_t>(pins_.clock_hz), timing.cs_setup_ns) ||
        (pins_.spi_mode != 0 && pins_.spi_mode != 3)) {
      ESP_LOGE(kTag, "clock, supply or SPI mode outside the AT25040B's bus requirements");
      return false;
    }
    const uint32_t hold_cycles =
        Esp32At25040bCsHoldCycles(static_cast<uint32_t>(pins_.clock_hz), timing.cs_hold_ns);
    if (hold_cycles > 16U) {
      ESP_LOGE(kTag, "t_CSH needs more than 16 SPI cycles at this clock");
      return false;
    }
    // CS idles high (deselected) before anything else on the bus runs: driven high before its
    // output is enabled, so the chip never sees a select it did not ask for (handbook KB-HW-018,
    // KB-HW-019). spi_bus_add_device() takes the pin over below.
    gpio_config_t cs_io{};
    cs_io.pin_bit_mask = uint64_t{1} << static_cast<uint32_t>(pins_.cs);
    cs_io.mode = GPIO_MODE_OUTPUT;
    if (gpio_set_level(static_cast<gpio_num_t>(pins_.cs), 1) != ESP_OK ||
        gpio_config(&cs_io) != ESP_OK) {
      return false;
    }
    if (pins_.wp >= 0) {
      // WP is driven to its idle level, ACTIVE (low, protected), before its output is enabled and
      // before the bus comes up, so the chip never sees it float high (handbook KB-HW-018).
      gpio_config_t io{};
      io.pin_bit_mask = uint64_t{1} << static_cast<uint32_t>(pins_.wp);
      io.mode = GPIO_MODE_OUTPUT;
      if (gpio_set_level(static_cast<gpio_num_t>(pins_.wp), 0) != ESP_OK ||
          gpio_config(&io) != ESP_OK) {
        return false;
      }
    }
    spi_bus_config_t bus{};
    bus.mosi_io_num = pins_.mosi;
    bus.miso_io_num = pins_.miso;
    bus.sclk_io_num = pins_.sclk;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = static_cast<int>(at25040b::kMaxTransferBytes);
    const esp_err_t bus_err = spi_bus_initialize(pins_.host, &bus, SPI_DMA_DISABLED);
    if (bus_err != ESP_OK && bus_err != ESP_ERR_INVALID_STATE) { // INVALID_STATE: bus already up
      ESP_LOGE(kTag, "spi_bus_initialize: %s", esp_err_to_name(bus_err));
      return false;
    }
    spi_device_interface_config_t dev{};
    dev.clock_speed_hz = pins_.clock_hz;
    dev.mode = static_cast<uint8_t>(pins_.spi_mode);
    dev.spics_io_num = pins_.cs;
    dev.queue_size = 1;
    dev.cs_ena_posttrans = static_cast<uint8_t>(hold_cycles); // t_CSH after the last bit
    if (spi_bus_add_device(pins_.host, &dev, &device_) != ESP_OK) {
      device_ = nullptr;
      return false;
    }
    return true;
  }

  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    if (device_ == nullptr || len == 0) {
      return false;
    }
    spi_transaction_t t{};
    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx; // nullptr when the driver needs no reply: MISO is discarded
    return spi_device_polling_transmit(device_, &t) == ESP_OK;
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }

  bool GpioSet(at25040b::CtrlPin pin, at25040b::GpioSignal signal) noexcept {
    if (pin != at25040b::CtrlPin::WriteProtect || pins_.wp < 0) {
      return false;
    }
    // WP is active low: ACTIVE (protected) is level 0.
    const uint32_t level = signal == at25040b::GpioSignal::ACTIVE ? 0U : 1U;
    return gpio_set_level(static_cast<gpio_num_t>(pins_.wp), level) == ESP_OK;
  }

  void Log(at25040b::LogLevel level, const char* message) noexcept {
    const esp_log_level_t esp_level = level == at25040b::LogLevel::Error     ? ESP_LOG_ERROR
                                      : level == at25040b::LogLevel::Warning ? ESP_LOG_WARN
                                      : level == at25040b::LogLevel::Info    ? ESP_LOG_INFO
                                                                             : ESP_LOG_DEBUG;
    esp_log_write(esp_level, kTag, "%s\n", message); // plain text, never a format
  }

private:
  static constexpr const char* kTag = "at25040b";
  Esp32At25040bPins pins_;
  spi_device_handle_t device_ = nullptr;
};

#endif // AT25040B_ESP32_AT25040B_PORT_HPP
