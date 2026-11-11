# Datasheet map

The driver follows the AT25010B/020B/040B datasheet. This page says which revision, where to get it, and where each
part of the driver is described in it, so you can check any behaviour against the source. Section names and page
numbers are the datasheet's own (the printed page numbers).

<!-- generated: datasheet-map -->
- **Datasheet: AT25010B, AT25020B, AT25040B SPI Serial EEPROM**, Microchip (Atmel) Atmel-8707, revision F (2015-01). Get
  it from the [vendor page](https://www.microchip.com/en-us/product/AT25040B) or as a
  [PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf).
  SHA-256 of the file used: `4783c65c38abe35ea0950a9918d02df63c592a30fd841f52d2d3233221d606b0`.

| Topic | Datasheet section | Page | In the driver | Explained on |
| --- | --- | --- | --- | --- |
| Array sizes, page size, write cycle and endurance | Features; Description | [p. 1](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=1) | `Variant`, `CapacityBytes()`, `kPageSizeBytes` | [Hardware](hardware.md), [Configuration](configuration.md) |
| Pins and packages | 1 Pin Configurations, Table 1-1 | [p. 2](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=2) | `CtrlPin::WriteProtect` | [Hardware](hardware.md), [Quickstart](quickstart.md) |
| Supply voltage and current | 4.2 DC Characteristics, Table 4-2 | [p. 4](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=4) | - | [Hardware](hardware.md) |
| SPI clock, CS timing and the write cycle time | 4.3 AC Characteristics, Table 4-3 | [p. 5](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=5) | `kMaxClockHz`, `kMinCsHighNs`, `kCsSetupNs`, `kCsHoldNs`, `kDataSetupNs`, `kDataHoldNs`, `BusTimingAt()`, `kWriteCycleMaxUs`, `Config::write_timeout_us` | [Hardware](hardware.md), [Porting](porting.md), [Configuration](configuration.md) |
| SPI modes, invalid opcodes, HOLD and the WP pin | 5 Serial Interface Description | [p. 6](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=6) | `kSpiMode`, `kMsbFirst`, `SpiInterface`, `GpioSet()`, `CtrlPin::WriteProtect`, `Error::Pin` | [Hardware](hardware.md), [Porting](porting.md), [Troubleshooting](troubleshooting.md) |
| Instruction set and A8 in the opcode | 6 Functional Description, Table 6-1 | [p. 8](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=8) | `inc/at25040b_registers.hpp`, `detail::EepromBus` | [Hardware](hardware.md), [Architecture](architecture.md) |
| Write enable latch, WREN and WRDI | 6 Functional Description, Write Enable (WREN), Write Disable (WRDI) | [p. 8](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=8) | `EnsureInitialized()`, `Write()`, `Error::WriteProtected` | [Contract](contract.md), [API guide](api-guide.md), [Troubleshooting](troubleshooting.md) |
| Status register, RDY and the all-ones status of a write cycle | 6 Functional Description, Tables 6-2 and 6-3 | [p. 8](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=8) | `ReadStatus()`, `ReadRawStatus()`, `Status`, `EnsureInitialized()`, `Error::NoDevice`, `Error::BusNoise` | [Hardware](hardware.md), [Contract](contract.md), [Troubleshooting](troubleshooting.md) |
| Block write protection (BP1, BP0) | 6 Functional Description, Write Status Register (WRSR), Table 6-4 | [p. 9](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=9) | `SetBlockProtection()`, `GetBlockProtection()`, `ProtectedFrom()`, `Error::BlockProtected` | [API guide](api-guide.md), [Hardware](hardware.md) |
| Sequential read and roll-over | 6 Functional Description, Read Sequence (READ) | [p. 9](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=9) | `Read()`, `kReadChunkBytes` | [API guide](api-guide.md), [Architecture](architecture.md) |
| Page write, the page wrap and the self-timed write cycle | 6 Functional Description, Write Sequence (WRITE) | [p. 9](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=9) | `Write()`, `Error::Timeout`, `Error::WriteMismatch` | [API guide](api-guide.md), [Architecture](architecture.md), [Contract](contract.md) |
| Instruction timing | 7 Timing Diagrams, Figures 7-1 to 7-8 | [p. 10](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=10) | `detail::EepromBus` | [Hardware](hardware.md), [Porting](porting.md) |
| Ordering codes and part markings | 8 Ordering Code Detail; 9 Part Markings | [p. 13](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=13) | `Config::variant` | [Hardware](hardware.md) |
<!-- /generated -->

## When the datasheet and the driver differ

The driver is written against the revision above. If you have a newer revision, compare its revision history with
the topics above; a change to a pin, a register, a timing or a value the driver uses is a bug report for this
repository. Where the driver deliberately goes beyond the datasheet, the [Contract](contract.md) says so.

The repository never contains the PDF itself: vendors own their documents. Download it from the vendor, and compare
its SHA-256 with the one above to confirm you have the same file.
