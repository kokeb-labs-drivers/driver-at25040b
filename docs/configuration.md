# Configuration

Every field of `at25040b::Config`, how a configuration is checked, and the optional Kconfig layer.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip (Atmel) Atmel-8707 F:

    - Array sizes, page size, write cycle and endurance: Features; Description, [p. 1](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=1)
    - SPI clock, CS timing and the write cycle time: 4.3 AC Characteristics, Table 4-3, [p. 5](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf#page=5)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Fields

The chip has no setting to configure: its only one, the block protection, is non-volatile and set with
`SetBlockProtection()`. `Config` describes the board instead: which part is fitted, how WP is wired, and whether the
start-up identity check runs. `Config{}` is an AT25040B with WP driven by the port (or tied high) and the identity
check on. Read and write verification is not a `Config` field: each call chooses it
([below](#verification-per-call)).

| Field | Default | Range | What it does |
| --- | --- | --- | --- |
| `variant` | `Variant::At25040b` | `At25010b`, `At25020b`, `At25040b` | The fitted part: 128, 256 or 512 bytes. Every address is checked against it; A8 is used only on the AT25040B |
| `check_write_enable` | `true` | | The write enable toggle at start-up ([Contract](contract.md#the-identity-check)). Runs once, in `EnsureInitialized()`, never on the `Read()` and `Write()` path; it is what tells a dead bus from a chip. Turn it off only when WP is tied low |
| `write_timeout_us` | 10000 | 5000 to 1000000 | Longest wait for a write cycle, in µs. t<sub>WC</sub> is 5 ms at most (Table 4-3) |

The chip cannot tell its size: a wrong `variant` either wastes the top of a larger part or, on a smaller part, lets
an address alias the start of the array. Read the marking ([Hardware](hardware.md#variants)) when in doubt.

**When to turn the check off.** `check_write_enable = false` is for a read-only board with WP tied low: the toggle
cannot pass there. It weakens the start-up check (a dead bus with MISO stuck low passes), so check your data with a
CRC.

```cpp
--8<-- "tests/docs/configuration.cpp:custom"
```

## Verification per call

`Read()` and `Write()` take a last argument, `at25040b::Verify`, default `Verify::None`: the call sends only what the
datasheet needs, plus the status reads that keep it off a busy chip.

| `Verify` | `Read()` | `Write()` |
| --- | --- | --- |
| `None` (default) | Each 16-byte chunk read once | No read-back; the WEN check after each write cycle still catches a WRITE the chip ignored |
| `ReadBack` | Each chunk read twice and compared (`BusNoise`): twice the READs | Each page read back and compared (`WriteMismatch`): one READ more per page |

Pass `Verify::ReadBack` where a corrupted transfer must never pass unnoticed and the data carries no CRC or signature
of its own. Where it does, the default is enough: the CRC
catches what a second read would.

## Validation

`at25040b::Validate(config)` checks a configuration without touching the chip, and `EnsureInitialized()` and
`ApplyConfig()` call it before anything else. It returns `Error::InvalidArgument` when `variant` is not an
enumerator or `write_timeout_us` is below 5000 or above 1000000.

`ApplyConfig()` never does I/O, before or after the start: the variant and the timeout apply from the next call,
`check_write_enable` from the next `EnsureInitialized()`.

## Kconfig

Under ESP-IDF, `idf.py menuconfig` shows **AT25010B / AT25020B / AT25040B SPI EEPROM** when the driver is a component
of the project ([Installation](installation.md)); the menu is [`_config/Kconfig`](../_config/Kconfig). The options
reach the code only through [`inc/at25040b_kconfig.hpp`](../inc/at25040b_kconfig.hpp), never through a driver
signature:

```cpp
--8<-- "tests/docs/configuration.cpp:kconfig"
```

| Option | Default | `Config` field |
| --- | --- | --- |
| `CONFIG_AT25040B_VARIANT_*` | AT25040B | `variant` |
| `CONFIG_AT25040B_SKIP_WRITE_ENABLE_CHECK` | n | `check_write_enable = false` when set |
| `CONFIG_AT25040B_WRITE_TIMEOUT_US` | 10000 | `write_timeout_us` |

The identity check is on by default, so its option is a "skip" option: Kconfig writes a bool set to "n" as an absent
macro, and an absent macro must mean "check". Verification has no Kconfig option: it is chosen per call. Without Kconfig
(plain CMake, other platforms) `KconfigDefaults()` returns `Config{}`. Both cases are host tests.

Next: [Porting](porting.md).
