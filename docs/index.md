---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# AT25040B Driver

<!-- generated: chip-summary -->
The **AT25040B** is a 1 Kbit, 2 Kbit or 4 Kbit serial EEPROM (128, 256 or 512 bytes) on an SPI bus: it keeps data with
the power off, writes a page in one self-timed cycle and protects a quarter, a half or all of the array (Features, [p.
1](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=1)).
<!-- /generated -->

<p>A C++20 driver for the Microchip (formerly Atmel) AT25010B, AT25020B and AT25040B SPI serial EEPROMs. It checks
that a chip really answers before trusting it, reads any range, writes page by page with the write enable latch,
the write cycle and a read-back checked at every step, drives the WP pin and sets the block protection, on any
MCU.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-at25040b){ .md-button }
</div>

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/chip-hero.dark.svg">
  <img src="diagrams/chip-hero.svg"
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
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/chip-capabilities.dark.svg">
  <img src="diagrams/chip-capabilities.svg"
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
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/chip-blocks.dark.svg">
  <img src="diagrams/chip-blocks.svg"
    alt="Block diagram of the AT25040B: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

## Pins and wiring

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/chip-pinout.dark.svg">
  <img src="diagrams/chip-pinout.svg"
    alt="Pinout of the AT25040B in SOIC-8, top view: 8 pins, each with its name, its electrical type as a
    colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/chip-wiring-typical.dark.svg">
  <img src="diagrams/chip-wiring-typical.svg"
    alt="Wiring of the AT25040B for the Typical wiring: MCU SPI SCK to SCK (Transfer()); MCU SPI MOSI to SI
    (Transfer()); MCU SPI MISO to SO (Transfer()); MCU GPIO / CS to CS (Transfer()); MCU GPIO to WP
    (GpioSet(CtrlPin::WriteProtect)). Supplies, grounds and the support parts are listed under the
    drawing.">
</picture>
<!-- /figure -->

<!-- figure: chip-frame-memory -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/chip-frame-memory.dark.svg">
  <img src="diagrams/chip-frame-memory.svg"
    alt="READ and WRITE instruction of the AT25040B, 24 bits, most significant bit first. READ: 0000 (bits 23
    to 20), A8 (bit 19), 011 (bits 18 to 16), address A7 to A0 (bits 15 to 8), any value (ignored) (bits
    7 to 0); Data out: high impedance (bits 23 to 8), data D7 to D0 (bits 7 to 0); WRITE: 0000 (bits 23
    to 20), A8 (bit 19), 010 (bits 18 to 16), address A7 to A0 (bits 15 to 8), data D7 to D0 (bits 7 to
    0). Below, the SPI mode 0 waveform of one transfer in the same bit grid.">
</picture>
<!-- /figure -->

<!-- figure: chip-frame-status -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/chip-frame-status.dark.svg">
  <img src="diagrams/chip-frame-status.svg"
    alt="RDSR and WRSR instruction of the AT25040B, 16 bits, most significant bit first. RDSR: 0000 X101
    (bits 15 to 8), any value (ignored) (bits 7 to 0); Status: high impedance (bits 15 to 8), status
    register (bits 7 to 0); WRSR: 0000 X001 (bits 15 to 8), status register (BP1 and BP0 in bits 3 and
    2) (bits 7 to 0). The status register field of the Status row: bit 7 X7, bit 6 X6, bit 5 X5, bit 4
    X4, bit 3 BP1, bit 2 BP0, bit 1 WEN, bit 0 RDY. Below, the SPI mode 0 waveform of one transfer in
    the same bit grid.">
</picture>
<!-- /figure -->

The pin tables, the full schematic and the timing are on [Hardware](hardware.md).

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

From [`docs/hardware.yml`](hardware.yml); `kokeb-docs hardware --check` confirms that every API name is in
`inc/at25040b.hpp`, `inc/at25040b_types.hpp`, `inc/at25040b_spi_interface.hpp` and every register in
`inc/at25040b_registers.hpp`.
<!-- /generated -->

The driver has no platform code. You give it a small class that brings the bus up, moves bytes and waits; it does
the rest: the identity check, the instruction framing with A8, the page split, the write cycle, the WP pin and the
checks that catch a dead or noisy bus.

## What you get

- **One object per chip.** `AT25040B<YourPort>` takes a reference to your port and does no I/O until
  `EnsureInitialized()`. See [API guide](api-guide.md).
- **A probe that a dead bus cannot pass.** The start-up check reads the status twice and toggles the write enable
  latch, so a floating, stuck or echoing MISO fails it. See [Contract](contract.md#the-identity-check).
- **Writes you can trust.** Page split, WREN per page, WEN checked, the write cycle waited for, each page read back.
  See [API guide](api-guide.md#write).
- **Errors you can act on.** Every call returns `Result<T>` with an `Error` such as `NoDevice`, `BusNoise` or
  `WriteProtected`. See [API guide](api-guide.md#handle-errors).
- **No heap, no exceptions, no RTTI.** The host tests build with `-fno-exceptions -fno-rtti`. See
  [Contract](contract.md).
- **Tested without hardware.** Host tests and a fuzzer run the driver against a protocol model of the chip. See
  [Testing and CI](testing.md). Not yet verified on hardware: [Status and limitations](limitations.md).

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: AT25010B, AT25020B, AT25040B SPI Serial EEPROM (Microchip (Atmel) Atmel-8707) | F, 2015-01 | [PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf) · [vendor page](https://www.microchip.com/en-us/product/AT25040B) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the chip, write the port, read the first bytes in about 15 lines.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, instructions, timing, WP and HOLD, block protection.

-   :material-file-document-check-outline: **[Contract](contract.md)**

    ---

    What the driver promises, what it needs, threading and errors.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Probe, read, write, protect, and use it from a handler.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the port for your MCU.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and member, generated from the headers.

</div>
