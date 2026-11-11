# Quickstart

Run the driver on your PC first, with no hardware; then read the chip from your MCU: wire it, write a small port
class, call two driver functions.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip (Atmel) Atmel-8707 F:

    - Pins and packages: 1 Pin Configurations, Table 1-1, [p. 2](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=2)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Try it on your PC

[`examples/host/`](../examples/host/README.md) runs the real driver against a simulated AT25040B, through a host
port. From the repository root, with CMake and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/at25040b_host_example
```

```text
AT25040B driver 0.1.0 on the host
at25040b: initialised
wrote 12 bytes at 0x0FC in 2 write cycles
read back: 6B 6F 6B 65 62 2D 61 74 32 35 00 01
at25040b: status reads 0xFF: nothing drives MISO
after unplugging: NoDevice, status byte 0xFF
```

The example checks the chip, writes 12 bytes across a page boundary (two write cycles), reads them back, then
unplugs the simulated module and probes again. The calls in [`main.cpp`](../examples/host/main.cpp) are the ones
below; only the port differs on an MCU. CI builds and runs this example on every pull request.

## 1. Wire it

| MCU | AT25040B (SOIC-8, TSSOP-8) | Notes |
| --- | --- | --- |
| SPI SCK | SCK (pin 6) | Mode 0 or 3; 1 MHz for bring-up on jumper wires |
| SPI MOSI | SI (pin 5) | |
| SPI MISO | SO (pin 2) | High impedance while CS is high, so the bus can be shared |
| A GPIO or the SPI CS | CS (pin 1) | Low for one instruction, high between instructions |
| A GPIO, or VCC | WP (pin 3) | Low blocks every write. A GPIO lets the driver release it only while it writes |
| VCC | HOLD (pin 7) | Tie high. Never strap it to CS |
| 1.8 V to 5.5 V | VCC (pin 8) | 100 nF to GND at the pin |
| GND | GND (pin 4) | |

[Hardware](hardware.md) has the clock limits per supply voltage and the other packages.

## 2. Write the port

The driver calls three hooks: bring the bus up, move bytes, wait.

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

[Porting](porting.md) has the whole contract, the optional WP and log hooks, and the ESP-IDF port.

## 3. Read the chip

```cpp
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

`EnsureInitialized()` is the only call that talks to the chip before the start: it checks that an AT25xxxB
answers (see [Contract](contract.md#the-identity-check)). Every other call returns `Error::NotInitialized` until it
has succeeded. If it fails, `GetLastStatusByte()` shows what the bus returned: 0xFF is a floating MISO, 0x05 an
echo of the opcode. [Troubleshooting](troubleshooting.md) has the rest.

Next: [API guide](api-guide.md) for writes, block protection and the handler pattern.
