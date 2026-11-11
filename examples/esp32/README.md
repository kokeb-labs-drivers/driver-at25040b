# AT25040B ESP32 examples

ESP-IDF applications that run the AT25040B driver on an ESP32-C6, built by CI on every pull request.

## Wiring (ESP32-C6 DevKitC-1)

| ESP32-C6 | AT25040B (SOIC-8) | Notes |
| --- | --- | --- |
| GPIO6 | SCK (pin 6) | SPI2, mode 0, 1 MHz in the example (at most 5 MHz with this port: t_CSS) |
| GPIO7 | SI (pin 5) | MOSI |
| GPIO2 | SO (pin 2) | MISO |
| GPIO10 | CS (pin 1) | Driven by spi_master, one transfer per instruction |
| GPIO11 | WP (pin 3) | Driven by the port; set `wp = -1` in `Esp32At25040bPins` when WP is tied |
| 3V3 | HOLD (pin 7) | Tie high. Never strap HOLD to CS: every status read then fails |
| 3V3 | VCC (pin 8) | 100 nF to GND at the pin |
| GND | GND (pin 4) | |

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh at25040b_basic_example Debug
./scripts/flash_app.sh flash_monitor at25040b_basic_example Debug
```

`idf.py menuconfig` shows "AT25010B / AT25020B / AT25040B SPI EEPROM" under Component config: the part, the
start-up check, the read and write verification and the write cycle timeout that `at25040b::KconfigDefaults()`
returns.

## Expected output

On an erased part (every byte 0xFF), first boot:

```text
I (312) at25040b_example: AT25040B driver 0.1.0
at25040b: initialised
I (315) at25040b_example: ff ff ff ff ff ff ff ff
I (322) at25040b_example: boot 1: Ok
I (1325) at25040b_example: present
```

Not verified on hardware: this output is what the code prints, not a capture from a board. `EnsureInitialized
failed: NoDevice (status byte 0xFF)` means nothing drives MISO: check CS, VCC and the SO wire first.
`DeviceIdMismatch (status byte 0x00)` with the module plugged in usually means WP does not reach pin 3.

## Files

| File | What it is |
| --- | --- |
| [`main/at25040b_basic_example.cpp`](main/at25040b_basic_example.cpp) | The example app |
| [`main/esp32_at25040b_port.hpp`](main/esp32_at25040b_port.hpp) | The ESP-IDF port: bus, WP, delay, log |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
