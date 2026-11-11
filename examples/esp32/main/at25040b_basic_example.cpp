/**
 * @file at25040b_basic_example.cpp
 * @brief Check the AT25040B from the menuconfig values, read the first bytes, write a boot counter
 * at the top of the array and read it back, then probe the chip once a second.
 *
 * Wiring: examples/esp32/README.md. Expected output: one "initialised" line, the first 8 bytes,
 * the counter, then one "present" line per second, or the error by name when the module is
 * unplugged. Every Result is checked and logged by name.
 */
#include <array>
#include <cstdint>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "at25040b.hpp"
#include "at25040b_kconfig.hpp"
#include "esp32_at25040b_port.hpp"

namespace {
constexpr const char* kTag = "at25040b_example";
constexpr uint16_t kCounterAddress = 0x1F8; // the last page of an AT25040B
} // namespace

extern "C" void app_main(void) {
  static Esp32At25040bPort port; // static: the driver keeps a reference
  static at25040b::AT25040B<Esp32At25040bPort> eeprom(port, at25040b::KconfigDefaults()); // no I/O

  ESP_LOGI(kTag, "AT25040B driver %s", at25040b::GetDriverVersion());
  const auto init = eeprom.EnsureInitialized();
  if (!init) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s (status byte 0x%02X)",
             at25040b::ToString(init.error), static_cast<unsigned>(eeprom.GetLastStatusByte()));
    return;
  }

  std::array<uint8_t, 8> head{};
  if (const auto r = eeprom.Read(0, head); r) {
    ESP_LOG_BUFFER_HEX(kTag, head.data(), head.size());
  } else {
    ESP_LOGE(kTag, "Read failed: %s", at25040b::ToString(r.error));
  }

  std::array<uint8_t, 4> counter{};
  if (eeprom.GetCapacityBytes() > kCounterAddress && eeprom.Read(kCounterAddress, counter)) {
    uint32_t boots = (static_cast<uint32_t>(counter[0]) << 24U) |
                     (static_cast<uint32_t>(counter[1]) << 16U) |
                     (static_cast<uint32_t>(counter[2]) << 8U) | counter[3];
    boots = boots == 0xFFFFFFFFU ? 1U : boots + 1U; // an erased part reads 0xFF
    counter = {static_cast<uint8_t>(boots >> 24U), static_cast<uint8_t>(boots >> 16U),
               static_cast<uint8_t>(boots >> 8U), static_cast<uint8_t>(boots)};
    // The counter has no CRC: read the page back (one READ more).
    const auto w = eeprom.Write(kCounterAddress, counter, at25040b::Verify::ReadBack);
    ESP_LOGI(kTag, "boot %u: %s", static_cast<unsigned>(boots), at25040b::ToString(w.error));
  }

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    // Probe again, as a handler does on attach: WRDI, WP parked, then the identity check.
    (void)eeprom.Deinitialize();
    const auto probe = eeprom.EnsureInitialized();
    if (probe) {
      ESP_LOGI(kTag, "present");
    } else {
      ESP_LOGW(kTag, "probe: %s (status byte 0x%02X)", at25040b::ToString(probe.error),
               static_cast<unsigned>(eeprom.GetLastStatusByte()));
    }
  }
}
