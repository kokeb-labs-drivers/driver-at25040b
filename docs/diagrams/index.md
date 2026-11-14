# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the AT25040B

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the AT25040B: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## What the AT25040B does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
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

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## AT25040B: Typical wiring

<!-- figure: chip-circuit-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-typical.dark.svg">
  <img src="chip-circuit-typical.svg"
    alt="Schematic of the AT25040B typical wiring: the chip with U2 MCU, C1 capacitor 100 nF, R1 resistor 10
    kΩ; the MCU wires carry the port hooks GpioSet(CtrlPin::WriteProtect), Transfer().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## AT25040B: READ and WRITE instruction

<!-- figure: chip-frame-memory -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-frame-memory.dark.svg">
  <img src="chip-frame-memory.svg"
    alt="READ and WRITE instruction of the AT25040B, 24 bits, most significant bit first. READ: 0000 (bits 23
    to 20), A8 (bit 19), 011 (bits 18 to 16), address A7 to A0 (bits 15 to 8), any value (ignored) (bits
    7 to 0); Data out: high impedance (bits 23 to 8), data D7 to D0 (bits 7 to 0); WRITE: 0000 (bits 23
    to 20), A8 (bit 19), 010 (bits 18 to 16), address A7 to A0 (bits 15 to 8), data D7 to D0 (bits 7 to
    0). Below, the SPI mode 0 waveform of one transfer in the same bit grid.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## AT25040B: RDSR and WRSR instruction

<!-- figure: chip-frame-status -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-frame-status.dark.svg">
  <img src="chip-frame-status.svg"
    alt="RDSR and WRSR instruction of the AT25040B, 16 bits, most significant bit first. RDSR: 0000 X101
    (bits 15 to 8), any value (ignored) (bits 7 to 0); Status: high impedance (bits 15 to 8), status
    register (bits 7 to 0); WRSR: 0000 X001 (bits 15 to 8), status register (BP1 and BP0 in bits 3 and
    2) (bits 7 to 0). The status register field of the Status row: bit 7 X7, bit 6 X6, bit 5 X5, bit 4
    X4, bit 3 BP1, bit 2 BP0, bit 1 WEN, bit 0 RDY. Below, the SPI mode 0 waveform of one transfer in
    the same bit grid.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## AT25040B

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the AT25040B, a 1 Kbit, 2 Kbit or 4 Kbit serial EEPROM (128, 256 or 512 bytes) on an SPI
    bus: it keeps data with the power off, writes a page in one self-timed cycle and protects a quarter,
    a half or all of the array. A MCU talks to it over SPI mode 0 or 3. Headline figures: 1.8 to 5.5 V
    supply voltage; 512 x 8 array of the AT25040B (128 or 256 bytes on the smaller parts); 20 MHz SCK at
    4.5 V and above (10 MHz from 2.5 V, 5 MHz from 1.8 V); 5 ms self-timed write cycle, at most; 1
    million write cycles.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## AT25040B pinout, SOIC-8

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the AT25040B in SOIC-8, top view: 8 pins, each with its name, its electrical type as a
    colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## Wiring: Typical wiring

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-typical.dark.svg">
  <img src="chip-wiring-typical.svg"
    alt="Wiring of the AT25040B for the Typical wiring: MCU SPI SCK to SCK (Transfer()); MCU SPI MOSI to SI
    (Transfer()); MCU SPI MISO to SO (Transfer()); MCU GPIO / CS to CS (Transfer()); MCU GPIO to WP
    (GpioSet(CtrlPin::WriteProtect)). Supplies, grounds and the support parts are listed under the
    drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the AT25040B core, which sends register reads and writes
    through the SPI port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/at25040b.hpp`](../../inc/at25040b.hpp)
- `SpiInterface` in [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)

## The identity check, and what a broken bus looks like

<!-- figure: identity-check -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="identity-check.dark.svg">
  <img src="identity-check.svg"
    alt="Left: the steps of EnsureInitialized, the status read until idle and confirmed, the write enable
    toggle or the double read of the first bytes, each with its errors. Right: each bus fault from the
    tests with the status byte it shows and the error it returns.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/identity-check.py`](../../docs/diagrams/figures/identity-check.py)

Drawn from:

- `initialize` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `checkWriteEnableToggle` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `checkStableHead` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `waitIdle` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `start-up fault tests` in [`tests/test_at25040b.cpp`](../../tests/test_at25040b.cpp)

## Six instructions, one transfer each

<!-- figure: instruction-frames -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="instruction-frames.dark.svg">
  <img src="instruction-frames.svg"
    alt="Byte layouts of WREN, WRDI, RDSR, WRSR, READ and WRITE: a one-byte opcode, then for RDSR the status
    byte, for WRSR the value, for READ and WRITE an address byte and data; bit 3 of the READ and WRITE
    opcodes carries address bit A8 on the AT25040B.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/instruction-frames.py`](../../docs/diagrams/figures/instruction-frames.py)

Drawn from:

- `opcodes` in [`inc/at25040b_registers.hpp`](../../inc/at25040b_registers.hpp)
- `detail::EepromBus` in [`inc/at25040b_spi_interface.hpp`](../../inc/at25040b_spi_interface.hpp)
- `kReadChunkBytes` in [`inc/at25040b_types.hpp`](../../inc/at25040b_types.hpp)

## One read through the layers

<!-- figure: one-read-through-the-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="one-read-through-the-layers.dark.svg">
  <img src="one-read-through-the-layers.svg"
    alt="One call through the layers: your code calls Read(0x1F0, buf) (16 bytes), the core exchanges frames
    with the chip through your port and returns the result to your code.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/one-read-through-the-layers.seq.yml`](../model/one-read-through-the-layers.seq.yml)

Drawn from:

- [`docs/model/one-read-through-the-layers.seq.yml`](../model/one-read-through-the-layers.seq.yml)

## Write(), step by step

<!-- figure: write-path -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="write-path.dark.svg">
  <img src="write-path.svg"
    alt="Flowchart of Write: check the range, read the status until idle, refuse a block-protected range,
    release WP, then per 8-byte page WREN and a WEN check, WRITE, the write cycle wait, a WEN check and
    a read-back; WP is parked at the end, and each step names the error it can return.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/write-path.py`](../../docs/diagrams/figures/write-path.py)

Drawn from:

- `Write` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `writeRange` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `writePage` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `enableWrite` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `finishWriteCycle` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `waitIdle` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
- `withWriteAccess` in [`src/at25040b.ipp`](../../src/at25040b.ipp)
