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
