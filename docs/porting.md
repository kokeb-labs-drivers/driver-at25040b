# Porting

Run the driver on a new MCU by writing one class: the port.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip (Atmel) Atmel-8707 F:

    - SPI clock, CS timing and the write cycle time: 4.3 AC Characteristics, Table 4-3, [p. 5](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=5)
    - SPI modes, invalid opcodes, HOLD and the WP pin: 5 Serial Interface Description, [p. 6](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=6)
    - Instruction timing: 7 Timing Diagrams, Figures 7-1 to 7-8, [p. 10](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=10)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver never calls your SDK. It calls hooks on a class you derive from `at25040b::SpiInterface<Self>` (in
[`inc/at25040b_spi_interface.hpp`](../inc/at25040b_spi_interface.hpp)). The base uses CRTP and declares no hook
itself; the driver template is constrained by the `at25040b::SpiPort` concept, so calls are resolved at compile
time (no virtual functions, nothing to allocate) and a missing or misdeclared hook is a compile error at the point
of use.

## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the bus up on the first call; return `true` at once afterwards. `false` makes the driver's `EnsureInitialized()` return `Bus` |
| `bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept` | yes | One full-duplex transfer of `len` bytes (1 to 18) with CS low for all of them and high after. `rx` is `nullptr` when the driver needs no reply. `false` on any bus error |
| `void DelayUs(uint32_t us) noexcept` | yes | Wait at least `us` microseconds: the status polls during a write cycle, 100 µs each |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | Drive WP (`CtrlPin::WriteProtect`): `ACTIVE` is pin low (protected), `INACTIVE` pin high; `false` if the pin write failed, which the driver returns as `Error::Pin` |
| `void Log(LogLevel, const char* message) noexcept` | no | Print one line; `message` is plain text, never a format string |

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

Every hook is checked with its exact types and `noexcept`: a `Transfer()` with a `uint16_t len`, a `DelayUs(uint16_t)`
or a hook without `noexcept` is a compile error, not a silent truncation.

## Bus requirements

[`inc/at25040b_spi_interface.hpp`](../inc/at25040b_spi_interface.hpp) states what the chip needs from the bus as
`constexpr` values (datasheet "Features", section 4.3 Table 4-3, section 5). The scalar values hold over the whole
supply range, 1.8 V to 5.5 V; `BusTimingAt(supply_mv)` returns the faster limits of a higher supply. Check your port's
settings against them with `static_assert`, as the port above and the [examples](examples.md) do.

| Constant | Value | Source |
| --- | --- | --- |
| `kSpiMode`, `kSpiModeAlternate` | 0 (tested), 3 | "Features": modes 0 and 3 |
| `kMsbFirst` | `true` | Section 5, "MSB" |
| `kMaxClockHz` | 5 MHz; `BusTimingAt()`: 10 MHz from 2.5 V, 20 MHz from 4.5 V | Table 4-3, f<sub>SCK</sub> |
| `kMinCsHighNs` | 200 ns; 100 ns from 2.5 V | Table 4-3, t<sub>CS</sub> |
| `kCsSetupNs`, `kCsHoldNs` | 200 ns; 100 ns from 2.5 V | Table 4-3, t<sub>CSS</sub>, t<sub>CSH</sub> |
| `kDataSetupNs`, `kDataHoldNs` | 80 ns; 40 ns from 2.5 V, 20 ns from 4.5 V | Table 4-3, t<sub>SU</sub>, t<sub>H</sub> |
| `kContinuousWord` | `false`: idle SCK between bytes is fine, CS stays low | Table 4-3 (no t<sub>WH</sub>/t<sub>WL</sub> maximum) |
| `kMaxTransferBytes` | 18: opcode, address, 16 data bytes | The driver's read chunk |

## Control pins

| Pin | Polarity | Idle level | When the driver changes it |
| --- | --- | --- | --- |
| WP (`CtrlPin::WriteProtect`) | Active low: `ACTIVE` = pin low = writes blocked | `ACTIVE` (low) | Released (`INACTIVE`, high) only for a write, a block-protection change and the identity check, parked `ACTIVE` after |
| CS | Active low | High (deselected) | Never by the driver: the port's SPI controller drives it, low for each transfer. Drive it high before any device on the bus is clocked |
| HOLD | Active low | High | Never: tie it high (a HOLD strapped to CS makes every status read fail) |

The port drives WP to its idle level, low, before it enables the pin's output and before the first transfer
(handbook KB-HW-018), so the chip never sees WP float high. For this pin the idle level is the asserted one: WP is a
protection, and a glitch or a reset must leave the array protected. The driver never drives HOLD: tie it high.

## Transfer buffers

The port only ever receives the driver's own two 18-byte transfer buffers, which are members of the driver object, never
a caller's buffer or a stack array. If your SPI controller uses DMA, place the driver object in memory the DMA can reach
(on ESP32, internal RAM, not a PSRAM task stack).

Rules the transfer must follow:

- **One call, one instruction.** Keep CS low for all `len` bytes and raise it after: every instruction starts on a CS
  falling edge, and a write cycle starts when CS rises after the WRITE. Never split a transfer into several CS
  periods, and never join two.
- **Mode 0 or 3, MSB first.** At most `BusTimingAt(supply_mv).max_clock_hz`: 20 MHz from 4.5 V, 10 MHz from 2.5 V,
  5 MHz from 1.8 V; start at 1 MHz on jumper wires. CS high for at least t<sub>CS</sub> between transfers.
- **Raw bytes only.** Do not interpret, retry or reorder bytes; the driver owns the framing and decides on retries.
- **Return `false` for a bus error.** The driver turns it into `Error::Bus` (a pin hook into `Error::Pin`) and never
  ignores it.
- **Bounded time-outs.** A stuck bus fails the call; it does not block forever.
- **Shared bus.** If other devices share the bus, take the bus lock in the port around each transfer, or in the
  handler around each driver call ([API guide](api-guide.md#use-it-from-a-handler)).

## Optional hooks

The driver finds the optional hooks at compile time with C++20 `requires`, in one place (`at25040b::detail`). A
missing hook falls back in a documented, safe way or fails to compile; it never skips a step silently.

| Hook | Used for | Without it |
| --- | --- | --- |
| `GpioSet()` | WP: released for a write, a block-protection change and the identity check, parked after | WP is the board's: tie it high for writes, or low for a read-only board (then `Config::check_write_enable = false`) |
| `Log()` | One line per lifecycle event or failure | Nothing is logged |

A port with every hook:

```cpp
--8<-- "tests/docs/ports.cpp:full-port"
```

```cpp
--8<-- "tests/docs/ports.cpp:wp-check"
```

A `GpioSet()` that returns `void`, and a port without `DelayUs()`, are compile errors: the driver must see a failed
pin write, and the write cycle wait is never skipped. The [compile-fail tests](testing.md) check both rules.

## Reference implementations

Every port lives with its example, in `examples/<platform>/`; the driver has none. [Platforms](platforms.md) lists
them and how to add one.

- [`examples/host/host_at25040b_port.hpp`](../examples/host/host_at25040b_port.hpp): any PC, against a simulated
  chip. Start from it for a host build or a test bench; it is also the smallest complete port.
- [`examples/esp32/main/esp32_at25040b_port.hpp`](../examples/esp32/main/esp32_at25040b_port.hpp): ESP-IDF, with
  every hook. Start from it for any ESP-IDF target.

## Check your port

1. Build it: the ports on this page are compiled by the host CI from [`tests/docs/`](../tests/docs/my_port.hpp),
   with the driver instantiated over them.
2. Call `EnsureInitialized()` with the chip fitted: `Ok` proves that transfers work both ways, that CS frames each
   instruction and, with `GpioSet()`, that WP reaches the chip. `GetLastStatusByte()` is 0x00 on a new part.
3. Unplug the chip (or lift CS) and call `Deinitialize()` then `EnsureInitialized()`: `NoDevice` or
   `DeviceIdMismatch`, never `Ok`.
4. Write 16 bytes across a page boundary to an unused range and read them back.

## Threads

The driver is not thread-safe. Give each driver object one owner task, or guard every call with one mutex. When
several chips share a bus, the port or the handler must also serialise the bus. See [Contract](contract.md#threading).

## Editor setup

The clang-format and clang-tidy configuration is in `_config/`, and the repository root has no copy of it. Tools
that take a path use it directly; CI does the same:

| Tool | Setting |
| --- | --- |
| Command line | `clang-format --style=file:_config/.clang-format`, `clang-tidy --config-file=_config/.clang-tidy -p build` |
| VS Code (C/C++ extension) | `"C_Cpp.clang_format_style": "file:${workspaceFolder}/_config/.clang-format"` and `"C_Cpp.codeAnalysis.clangTidy.args": ["--config-file=${workspaceFolder}/_config/.clang-tidy"]` |
| clangd (VS Code clangd, Neovim, Emacs), CLion | These find `.clang-format` and `.clang-tidy` only by name, from the file upwards: copy both to the root once (`cp _config/.clang-format _config/.clang-tidy .`). `.gitignore` lists the copies, so they never reach a commit; copy again after `_config/` changes |

Configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (clangd: `--compile-commands-dir=build`) so the editor sees the
same flags as the build.
