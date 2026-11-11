# Troubleshooting

Symptoms you may see while bringing up an AT25010B, AT25020B or AT25040B, their causes and the fix for each.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip (Atmel) Atmel-8707 F:

    - SPI modes, invalid opcodes, HOLD and the WP pin: 5 Serial Interface Description, [p. 6](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=6)
    - Write enable latch, WREN and WRDI: 6 Functional Description, Write Enable (WREN), Write Disable (WRDI), [p. 8](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=8)
    - Status register, RDY and the all-ones status of a write cycle: 6 Functional Description, Tables 6-2 and 6-3, [p. 8](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=8)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

Start with `GetLastStatusByte()` after a failed call: it tells a floating bus, a looped-back bus and a working chip
apart without a logic analyser.

## EnsureInitialized fails

| Error and status byte | Likely cause | Fix |
| --- | --- | --- |
| `NoDevice`, 0xFF | Nothing drives MISO: module unplugged, CS on the wrong pin, no VCC, SO not connected | Check SO (pin 2) to MISO, CS (pin 1), VCC (pin 8) |
| `NoDevice`, 0xFF, wiring right | HOLD low during the transfer: HOLD floating or strapped to CS | Tie HOLD (pin 7) to VCC |
| `NoDevice` right after an MCU reset | A write cycle that never ends (a damaged part) | Power-cycle the module; a real write cycle ends in 5 ms and the driver waits for it |
| `DeviceIdMismatch`, 0x05 | MISO repeats MOSI one byte late: SI shorted to SO, or a loop-back | Look for a short between pins 5 and 2 |
| `DeviceIdMismatch`, 0x00 | MISO stuck or pulled low with no chip answering: SO is high impedance when no chip drives it (section 5) | Check SO continuity and the module's supply |
| `DeviceIdMismatch`, 0x00, chip present | WP low: the chip ignores WREN, so WEN never reads 1 | With `GpioSet()`: check the WP pin mapping and polarity (ACTIVE is low). WP tied low on purpose: set `Config::check_write_enable = false` |
| `DeviceIdMismatch`, another value | Wrong SPI mode, or a shifted bit | Use mode 0 or 3; CS must stay low for the whole transfer |
| `BusNoise` | Two status reads differed: another device drives MISO, or the line is noisy | Check the other chip selects on the bus; lower the clock |
| `Bus` | The port's `EnsureInitialized()` or a transfer returned `false` | Log in the port which one; check the bus is up and the lock is free |
| `Pin` | The port's `GpioSet()` could not drive WP | Check the WP GPIO number and that the port configured it as an output |
| `InvalidArgument` | `Config` fails `Validate()` | `write_timeout_us` from 5000 to 1000000, a real `variant` |

## Reads and writes fail

| Symptom | Cause and fix |
| --- | --- |
| `Timeout` after the start | The status stays 0xFF: the module was unplugged since the start (probe again), or a write cycle does not end (a failing part) |
| `BusNoise` from `Read()` | The two reads of a chunk differed: contention or noise on MISO. Lower the clock, shorten the wires, check the other chip selects |
| `WriteProtected` | WREN did not set WEN: WP low. Check `GpioSet()` and the WP wiring; a board that ties WP low cannot write |
| `BlockProtected` | The range is in the protected top of the array: `GetBlockProtection()` says how much. Lower it with `SetBlockProtection()` if you mean to |
| `WriteMismatch` from `Write()` | The read-back differed, or WEN was still set after the WRITE (the chip did not take it). Retry once; repeated on the same address means a worn cell (1,000,000 cycles) |
| `WriteMismatch` from `SetBlockProtection()` | BP1:BP0 did not take: WP dropped during the call, or the WRSR frame was lost |
| `InvalidArgument` from `Read()` or `Write()` | An empty span, or `address + size` past `GetCapacityBytes()`. Check `Config::variant` |
| Data at 0x100 and up reads like 0x000 | An AT25010B or AT25020B configured as AT25040B: set `Config::variant` |
| Reads fine, writes fail at a high clock | The clock is above the limit for your VCC: 5 MHz at 1.8 V, 10 MHz at 2.5 V, 20 MHz at 4.5 V |
| Random failures, worse with long wires | HOLD floating, or no decoupling: tie HOLD high, 100 nF at VCC |

## Still stuck

Capture one `EnsureInitialized()` with a logic analyser on CS, SCK, SI and SO. You should see: RDSR (`05 00`) twice
with the reply 0x00 in the second byte, WREN (`06`), RDSR with 0x02, WRDI (`04`), RDSR with 0x00, and WP high from just
before the WREN to just after the last RDSR. Compare with [Hardware](hardware.md#instructions).
