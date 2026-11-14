# AT25040B Driver

<!-- generated: chip-summary -->
The **AT25040B** is a 1 Kbit, 2 Kbit or 4 Kbit serial EEPROM (128, 256 or 512 bytes) on an SPI bus: it keeps data with
the power off, writes a page in one self-timed cycle and protects a quarter, a half or all of the array (Features, [p.
1](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=1)).
<!-- /generated -->

C++20 driver for the Microchip AT25010B, AT25020B and AT25040B SPI serial EEPROMs.

[![CI](https://github.com/kokeb-labs-drivers/driver-at25040b/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kokeb-labs-drivers/driver-at25040b/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-informational)](docs/installation.md#requirements)

**Documentation: [`docs/`](docs/index.md)**, with the API reference built by Doxygen ([how](docs/api-reference.md)).
The site at <https://kokeb-labs-drivers.github.io/driver-at25040b/> opens when the repository is public.

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-hero.dark.svg">
  <img src="docs/diagrams/chip-hero.svg"
    alt="Overview of the AT25040B, a 1 Kbit, 2 Kbit or 4 Kbit serial EEPROM (128, 256 or 512 bytes) on an SPI
    bus: it keeps data with the power off, writes a page in one self-timed cycle and protects a quarter,
    a half or all of the array. A MCU talks to it over SPI mode 0 or 3. Headline figures: 1.8 to 5.5 V
    supply voltage; 512 x 8 array of the AT25040B (128 or 256 bytes on the smaller parts); 20 MHz SCK at
    4.5 V and above (10 MHz from 2.5 V, 5 MHz from 1.8 V); 5 ms self-timed write cycle, at most; 1
    million write cycles.">
</picture>
<!-- /figure -->

## What it does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-capabilities.dark.svg">
  <img src="docs/diagrams/chip-capabilities.svg"
    alt="Capability map of the AT25040B: Sequential read (supported): Reads one byte or the whole array in
    one transfer; the address counter rolls over at the top; Page write (supported): Writes up to an
    8-byte page in one self-timed cycle, split at page boundaries, no erase first; Status register
    (supported): Reads ready/busy, the write enable latch and the block protection bits; Block write
    protection (supported): Makes the top quarter, the top half or all of the array read-only, kept
    without power; Write protect and latch (supported): Blocks writes with the WP pin and the write
    enable latch; the driver releases WP only while it writes; Three densities (supported): One driver
    for the 128, 256 and 512 byte parts; the AT25040B carries address bit A8 in the opcode; SPI modes 0
    and 3 (supported): Publishes the mode, bit order and clock and select timing for each supply
    voltage; HOLD pin (not covered): Pauses a transfer while SCK is low; the driver never uses it, tie
    the pin to VCC.">
</picture>
<!-- /figure -->

## Where it is used

<!-- generated: chip-uses -->
- Industrial and commercial equipment: where low power and low-voltage operation are essential

Uses named by the datasheet (Features, [p. 1](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=1)).
<!-- /generated -->

## Inside the chip

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-blocks.dark.svg">
  <img src="docs/diagrams/chip-blocks.svg"
    alt="Block diagram of the AT25040B: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

## Pins and wiring

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-pinout.dark.svg">
  <img src="docs/diagrams/chip-pinout.svg"
    alt="Pinout of the AT25040B in SOIC-8, top view: 8 pins, each with its name, its electrical type as a
    colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-wiring-typical.dark.svg">
  <img src="docs/diagrams/chip-wiring-typical.svg"
    alt="Wiring of the AT25040B for the Typical wiring: MCU SPI SCK to SCK (Transfer()); MCU SPI MOSI to SI
    (Transfer()); MCU SPI MISO to SO (Transfer()); MCU GPIO / CS to CS (Transfer()); MCU GPIO to WP
    (GpioSet(CtrlPin::WriteProtect)). Supplies, grounds and the support parts are listed under the
    drawing.">
</picture>
<!-- /figure -->

| MCU | AT25040B |
| --- | --- |
| SPI SCK, MOSI, MISO | SCK (pin 6), SI (pin 5), SO (pin 2) |
| Chip select | CS (pin 1): low for one instruction, high between instructions |
| A GPIO (or VCC) | WP (pin 3): low blocks writes; on a GPIO the driver releases it only while it writes |
| VCC | HOLD (pin 7): tie it high, never to CS |
| 1.8 V to 5.5 V, GND | VCC (pin 8) with 100 nF to GND, GND (pin 4) |

<!-- generated: chip-card -->
| | | Datasheet |
| --- | --- | --- |
| **Package** | SOIC-8 | 1, [p. 2](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=2) |
| **Pins** | 8 | 1, [p. 2](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=2) |
| **Variants** | `tssop`, `udfn`, `xdfn`, `vfbga` (below) | 10, [p. 16](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=16) |
| **Supply VCC** | 1.8 V to 5.5 V | 4.2, [p. 4](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=4) |

Also sold as:

| Variant | Package | Pins | Size | Ordering | Note | Datasheet |
| --- | --- | --- | --- | --- | --- | --- |
| `tssop` | TSSOP-8 | 8 | 4.40 mm body | AT25040B-XHL | - | 10, [p. 16](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=16) |
| `udfn` | UDFN-8 | 8 | 2.00 x 3.00 mm body, 0.50 mm pitch | AT25040B-MAHL | - | 10, [p. 16](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=16) |
| `xdfn` | XDFN-8 | 8 | 1.80 x 2.20 mm body, 0.40 mm pitch | AT25040B-MEHL | - | 10, [p. 16](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=16) |
| `vfbga` | VFBGA-8 | 8 | 1.50 x 2.00 mm body, 0.50 mm pitch | AT25040B-CUL | - | 10, [p. 16](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=16) |

From [`docs/hardware.yml`](docs/hardware.yml); pins, circuits and the block diagram: [Hardware](docs/hardware.md).
<!-- /generated -->

<!-- figure: chip-frame-memory -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-frame-memory.dark.svg">
  <img src="docs/diagrams/chip-frame-memory.svg"
    alt="READ and WRITE instruction of the AT25040B, 24 bits, most significant bit first. READ: 0000 (bits 23
    to 20), A8 (bit 19), 011 (bits 18 to 16), address A7 to A0 (bits 15 to 8), any value (ignored) (bits
    7 to 0); Data out: high impedance (bits 23 to 8), data D7 to D0 (bits 7 to 0); WRITE: 0000 (bits 23
    to 20), A8 (bit 19), 010 (bits 18 to 16), address A7 to A0 (bits 15 to 8), data D7 to D0 (bits 7 to
    0). Below, the SPI mode 0 waveform of one transfer in the same bit grid.">
</picture>
<!-- /figure -->

<!-- figure: chip-frame-status -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-frame-status.dark.svg">
  <img src="docs/diagrams/chip-frame-status.svg"
    alt="RDSR and WRSR instruction of the AT25040B, 16 bits, most significant bit first. RDSR: 0000 X101
    (bits 15 to 8), any value (ignored) (bits 7 to 0); Status: high impedance (bits 15 to 8), status
    register (bits 7 to 0); WRSR: 0000 X001 (bits 15 to 8), status register (BP1 and BP0 in bits 3 and
    2) (bits 7 to 0). The status register field of the Status row: bit 7 X7, bit 6 X6, bit 5 X5, bit 4
    X4, bit 3 BP1, bit 2 BP0, bit 1 WEN, bit 0 RDY. Below, the SPI mode 0 waveform of one transfer in
    the same bit grid.">
</picture>
<!-- /figure -->

## Quick start

**1. Wire it** as in [Pins and wiring](#pins-and-wiring).

**2. Write the port**: a class with `EnsureInitialized()` (bring the bus up), `Transfer()` (one full-duplex
transfer with CS low) and `DelayUs()`, about 25 lines on most SDKs; add `GpioSet()` for the WP pin.
[Porting](docs/porting.md) has the contract and a complete example.

**3. Read the chip:**

<!-- generated: quickstart-code -->
```cpp
#include <array>
#include <cstdint>

#include "at25040b.hpp"

int read_board_id(MyPort& port, std::array<uint8_t, 8>& id) {
  at25040b::AT25040B<MyPort> eeprom(port); // AT25040B, identity check on; no I/O yet

  if (auto r = eeprom.EnsureInitialized(); !r) { // 1. bus up, identity check
    return static_cast<int>(r.error); //    NoDevice: nothing on MISO. DeviceIdMismatch: wiring, WP
  }
  if (auto r = eeprom.Read(0, id); !r) { // 2. eight bytes from address 0
    return static_cast<int>(r.error);    //    Timeout: a write cycle that never ends
  }
  return 0; // 3. id holds what the chip sent: one READ, so check its CRC
}
```
<!-- /generated -->

`EnsureInitialized()` is the only call that starts the driver, and the only one that probes: the constructor clocks
nothing, so a board can build it at boot and probe later, when the module is plugged in. Every other call returns
`Error::NotInitialized` until the probe has succeeded.

## Driver coverage

<!-- generated: chip-coverage -->
| Capability | What the chip does | Driver API | Registers | Status | Datasheet |
| --- | --- | --- | --- | --- | --- |
| Sequential read | Reads one byte or the whole array in one transfer; the address counter rolls over at the top | `Read` | `kRead`, `kOpcodeA8` | Supported | 6, [p. 9](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=9) |
| Page write | Writes up to an 8-byte page in one self-timed cycle, split at page boundaries, no erase first | `Write`, `Verify::ReadBack` | `kWrite`, `kWren` | Supported | 6, [p. 9](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=9) |
| Status register | Reads ready/busy, the write enable latch and the block protection bits | `ReadStatus`, `GetLastStatusByte` | `kRdsr`, `kStatusRdy`, `kStatusWen`, `kStatusBusyByte` | Supported | 6, [p. 8](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=8) |
| Block write protection | Makes the top quarter, the top half or all of the array read-only, kept without power | `SetBlockProtection`, `GetBlockProtection`, `BlockProtect::UpperHalf` | `kWrsr`, `kStatusBp0`, `kStatusBp1` | Supported | 6, [p. 9](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=9) |
| Write protect and latch | Blocks writes with the WP pin and the write enable latch; the driver releases WP only while it writes | `GpioSet`, `CtrlPin::WriteProtect` | `kWren`, `kWrdi` | Supported | 6, [p. 8](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=8) |
| Three densities | One driver for the 128, 256 and 512 byte parts; the AT25040B carries address bit A8 in the opcode | `Variant`, `GetCapacityBytes` | `kOpcodeA8`, `kAddressA8` | Supported | 6, [p. 8](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=8) |
| SPI modes 0 and 3 | Publishes the mode, bit order and clock and select timing for each supply voltage | `kSpiMode`, `kSpiModeAlternate`, `kMaxClockHz`, `BusTimingAt` | - | Supported | 4.3, [p. 5](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=5) |
| HOLD pin | Pauses a transfer while SCK is low; the driver never uses it, tie the pin to VCC | - | - | Not covered | 5, [p. 6](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=6) |

**Supported**: the driver covers it and a host test checks it; **Not covered**: the chip has it and the driver leaves it
alone.

From [`docs/hardware.yml`](docs/hardware.yml); `kokeb-docs hardware --check` confirms that every API name is in
`inc/at25040b.hpp`, `inc/at25040b_types.hpp`, `inc/at25040b_spi_interface.hpp` and every register in
`inc/at25040b_registers.hpp`.
<!-- /generated -->

This driver checks that a chip really answers before it trusts the bus, reads any range, writes page by page with the
write enable latch and the write cycle checked (and, when a call asks for it, a read-back), drives the WP pin only while
it writes, and sets the block protection. It runs on any MCU: you give it a small class that moves bytes on your SPI bus
and waits, and it does the rest.

| | |
| --- | --- |
| **Chips** | Microchip (Atmel) AT25010B (128 B), AT25020B (256 B), AT25040B (512 B); SOIC, TSSOP, UDFN, XDFN, VFBGA; 1.8 V to 5.5 V |
| **Memory** | 8-byte pages, self-timed write cycle of 5 ms at most, 1,000,000 write cycles |
| **Bus** | SPI mode 0 or 3; 20 MHz from 4.5 V, 10 MHz from 2.5 V, 5 MHz from 1.8 V |
| **Protection** | WP pin (driven by the driver if your port has the hook), write enable latch, non-volatile block protection |
| **Driver** | Header-only C++20; no heap, exceptions, RTTI or virtual calls; every call returns `Result<T>` |
| **Runs on** | Any MCU with SPI and a C++20 compiler; examples for ESP32 (ESP-IDF) and the host |
| **Status** | 0.1.0, **not verified on hardware**: host-tested against a protocol model of the chip and fuzzed. See what is and is not verified ([limitations](docs/limitations.md)) |

- Checks the chip before trusting it: the status read until idle and confirmed, then the write enable latch toggled
  (WREN, WEN = 1, WRDI, WEN = 0) with nothing programmed. A floating, stuck or echoing MISO cannot pass: a stuck-low
  line reads 0x00, the same as an idle chip (Table 6-3), but it can never show WEN = 1.
- Reads any range in 16-byte transfers; waits out a running write cycle first. `Verify::ReadBack` reads each
  transfer twice and compares.
- Writes any range: split at 8-byte pages, WREN and a WEN check per page, the write cycle waited for, the latch
  checked again; `Verify::ReadBack` reads each page back. A protected range is refused before anything is sent.
- Sends only what the datasheet needs by default: the extra read-back is opt-in, per call.
- Drives WP through your port: high only for a write, a block-protection change and the probe, parked low after,
  on failure too.
- Sets and reads the non-volatile block protection (a quarter, a half or all of the array).
- Supports the three sizes from one `Config::variant`, with A8 in the opcode on the AT25040B.
- Takes `std::span` buffers and checks every length against the transfer buffers before anything is sent; publishes
  the chip's SPI requirements (mode, bit order, clock and CS timing per supply voltage) as `constexpr` values that a
  port checks with `static_assert`.
- Offers a single-shot raw status read (`ReadRawStatus()`) and the last status byte (`GetLastStatusByte()`) for
  bring-up, and reports a failed WP pin as `Pin`, distinct from a failed transfer (`Bus`).

## Documentation

| Page | Read it when you want to |
| --- | --- |
| [Quickstart](docs/quickstart.md) | Go from a bare chip to its first bytes on your MCU |
| [Hardware](docs/hardware.md) | Wire it, pick the clock, understand the instructions, WP, HOLD and block protection |
| [Porting](docs/porting.md) | Write the port class for your MCU's SDK |
| [API guide](docs/api-guide.md) | Use each call: probe, read, write, protect, use it from a handler, handle errors |
| [Configuration](docs/configuration.md) | Choose the part and the checks |
| [Troubleshooting](docs/troubleshooting.md) | Find the cause of an error from the status byte |
| [Contract](docs/contract.md) | Know exactly what the driver promises, needs and returns, and how the identity check works |
| [Status and limitations](docs/limitations.md) | Decide whether it is ready for your use: what is and is not verified, and the bench checks to run |
| [Datasheet map](docs/datasheet.md) | Find the datasheet section behind each part of the driver |
| [API reference](docs/api-reference.md) | Look up a class or member (Doxygen; built by CI and locally) |

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: AT25010B, AT25020B, AT25040B SPI Serial EEPROM (Microchip (Atmel) Atmel-8707) | F, 2015-01 | [PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf) · [vendor page](https://www.microchip.com/en-us/product/AT25040B) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](docs/datasheets.yml). Where each
part of the driver is in the datasheet: [Datasheet map](docs/datasheet.md).
<!-- /generated -->

Every docs page lists the datasheet sections it relies on, and code comments cite the section they implement.

## Try it on your PC

No hardware needed: [`examples/host`](examples/host/README.md) runs the real driver against a simulated chip. You need
git, CMake 3.16+ and a C++20 compiler (GCC 10+, Clang 13+ or MSVC 2022).

1. Get the source:

   ```bash
   git clone https://github.com/kokeb-labs-drivers/driver-at25040b.git && cd driver-at25040b
   ```

2. Build and run the example:

   ```bash
   cmake -S examples/host -B build-example
   cmake --build build-example
   ./build-example/at25040b_host_example
   ```

3. You should see the chip checked, a record written across a page boundary and read back, and the simulated module
   unplugged:

   ```text
   AT25040B driver 0.1.0 on the host
   at25040b: initialised
   wrote 12 bytes at 0x0FC in 2 write cycles
   read back: 6B 6F 6B 65 62 2D 61 74 32 35 00 01
   at25040b: status reads 0xFF: nothing drives MISO
   after unplugging: NoDevice, status byte 0xFF
   ```

To run the whole test suite instead: `cmake -S . -B build -DAT25040B_BUILD_TESTS=ON && cmake --build build && ctest
--test-dir build`.

## Add it to your project

Pick the row that matches your build; each is a few steps, and [Installation](docs/installation.md) has the details.

| Your build | Steps below |
| --- | --- |
| Plain CMake: STM32 (CubeMX), RP2040 (Pico SDK), nRF Connect, a host app | [A](#a-cmake-any-mcu) |
| ESP-IDF | [B](#b-esp-idf) |
| An SDK or OS image that installs libraries | [C](#c-installed-package) |

### A. CMake, any MCU

1. Add the driver as a submodule, pinned to the version you chose:

   ```bash
   git submodule add https://github.com/kokeb-labs-drivers/driver-at25040b.git external/driver-at25040b
   ```

2. In your `CMakeLists.txt`, after `project()`:

   ```cmake
   add_subdirectory(external/driver-at25040b)
   target_link_libraries(my_app PRIVATE at25040b::at25040b)   # brings the headers and C++20
   ```

3. In your code, `#include "at25040b.hpp"`, write the port ([Porting](docs/porting.md)) and build. A first build
   that compiles the quickstart above is the check that the driver is wired in.

### B. ESP-IDF

1. Add the driver under your project's `components/`; ESP-IDF finds it there and names it after the folder:

   ```bash
   git submodule add https://github.com/kokeb-labs-drivers/driver-at25040b.git components/at25040b
   ```

2. In `main/CMakeLists.txt`, list it: `idf_component_register(SRCS "main.cpp" REQUIRES at25040b)`.
3. `idf.py menuconfig` now has an **AT25010B / AT25020B / AT25040B SPI EEPROM** menu (the part, the identity check,
   the write timeout), and `idf.py build` builds the driver with your app.
   [`examples/esp32`](examples/esp32) is a complete project with an SPI port and WP on a GPIO.

### C. Installed package

1. Install it once into a prefix:

   ```bash
   cmake -S external/driver-at25040b -B build-at25040b && cmake --install build-at25040b --prefix /opt/sdk
   ```

2. In your project, with `CMAKE_PREFIX_PATH=/opt/sdk`:

   ```cmake
   find_package(at25040b 0.1 REQUIRED)
   target_link_libraries(my_app PRIVATE at25040b::at25040b)
   ```

The target is `at25040b::at25040b` every way, so switching later changes nothing else.

## Tests and quality

```bash
cmake -S . -B build -DAT25040B_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
```

The host tests run the driver against a protocol model of the chip with the faults a board can produce (MISO floating, stuck
low or echoing, HOLD on CS, WP low, a WRITE the chip ignores), fail every bus transfer and WP pin write of every
operation in turn, and produce each error the driver can return. A fuzzer fails, loses, corrupts and replaces the chip's
replies and checks the driver's promises after every call, among them that no `Verify::ReadBack` call returns `Ok` for
bytes the chip does not hold after one corrupted transfer and that WP is always parked. CI also builds for Cortex-M0+ to
M33F and ESP32, and gates coverage, complexity, sanitizers, fuzzing, clang-tidy, cppcheck and the [Kokeb Labs
handbook](https://github.com/kokeb-labs/handbook) rules. See [Testing](docs/testing.md), which lists every gate and its
limit.

## License

MIT. See [LICENSE](LICENSE).
