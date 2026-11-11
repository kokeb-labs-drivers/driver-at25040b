# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Changed

- The repository moved to kokeb-labs-drivers; URLs follow.
- The Hardware page draws the chip from the new `docs/hardware.yml`: the SOIC-8 pinout with each pin's port hook,
  the block diagram of Figure 3-1, the SPI wiring of Figure 5-1 with WP on a GPIO, and the pin and parts tables,
  every fact with its datasheet section and page. Values the datasheet does not give (the decoupling capacitor and
  the WP pull-down) are marked as not confirmed. The hand-written pin table is gone.
- The `hardware-lessons-review`, `new-driver-from-template` and `verify` skills follow the template, and
  `tools/preflight.sh` runs every local check before a push. The chip model's noise generator is parenthesised for
  the clang-tidy check that script now runs.
- The `misra-review`, `cpp20-profile` and `new-driver-from-template` skills and `docs/repo-setup.md` follow the
  template: reading the MISRA report, the known tool misfires, enumeration ranges without a sentinel, and the
  settings tool.

## [0.1.0] - 2026-11-09

### Breaking

- `Read()` and `Write()` take a `std::span` instead of a pointer and a length:
  `Read(uint16_t address, std::span<uint8_t> out, Verify)` and `Write(uint16_t address, std::span<const uint8_t> data,
  Verify)`. Migration: `Read(a, buf.data(), buf.size())` becomes `Read(a, buf)` (a `std::array` or a C array converts
  to the span), and `Read(a, p, n)` becomes `Read(a, std::span{p, n})`. The null-pointer case is gone; an empty span is
  still `InvalidArgument`.
- A failed WP pin hook (`GpioSet()`) is reported as the new `Error::Pin` (21), the name the handbook gives a failed
  pin hook (KB-CPP-047), instead of `Error::Bus`, from `EnsureInitialized()`, `Write()`, `SetBlockProtection()` and
  `Deinitialize()`. `Deinitialize()` now returns `Bus` for a failed WRDI and `Pin` for a failed park, instead of `Bus`
  for both. When a step fails and parking WP fails too, any call returns `Pin` (WP may be left released) and logs it.
  A handler that mapped `Bus` to "check the pin" maps `Pin` now.
- The port concept checks every hook's exact types and `noexcept`: a `Transfer()` with a narrower length, a
  `DelayUs()` with a narrower parameter, or any hook without `noexcept`, is now a compile error. `Error` is
  `[[nodiscard]]`, so a discarded `Validate()` result warns.
- `GetConfig()` can no longer be called on a temporary driver (`const&` overload only).

### Added

- The chip's SPI requirements as `constexpr` values in `inc/at25040b_spi_interface.hpp` (handbook KB-CPP-123):
  `kSpiMode` (0, and `kSpiModeAlternate` 3), `kMsbFirst`, `kMaxClockHz`, `kMinCsHighNs`, `kCsSetupNs`, `kCsHoldNs`,
  `kDataSetupNs`, `kDataHoldNs`, `kContinuousWord`, valid over the whole supply range, and `BusTimingAt(supply_mv)`
  with the faster limits from 2.5 V and 4.5 V (datasheet Table 4-3). The host, ESP32 and docs ports check their
  settings against them with `static_assert`; the ESP32 port also checks its clock, mode and t_CSS at start, sets the
  CS hold time from the clock and drives CS high before the bus comes up (at most 5 MHz with that port).
- `ReadRawStatus()`: one RDSR, no wait, no retry, no check (handbook KB-CPP-129).
- Strict-dialect builds (KB-CPP20-001): `CMAKE_CXX_EXTENSIONS OFF` and `-pedantic-errors` in every folder that builds
  a target, the `at25040b_header_checks` target (each public header alone as ISO C++20, also in the Cortex-M
  cross-build) and `examples/esp32/main/at25040b_cxx20_check.cpp` inside ESP-IDF.
- `tests/test_model.cpp`: the chip model now carries its own register map and Table 6-4 from the datasheet and
  includes no driver header; the test cross-checks both maps at compile time (KB-TST-030).
- Compile-fail tests for a narrowed `Transfer()` length, a `Transfer()` without `noexcept` and a narrowed `DelayUs()`.
- From the template: the `cpp20-profile`, `misra-review` and `hardware-lessons-review` skills and the
  `safety-reviewer` agent.

### Changed

- MISRA C++:2023 conformance: `std::fill` replaces the discarded result of `std::ranges::fill` (Rule 0.1.2); the
  typed literals `_u8`, `_u16` and `_u32` (`detail`, out-of-range values do not compile) and explicit widening casts
  for constants and arguments of another type (Rule 7.0.6); both operands widened to `uint32_t` in a bitwise OR of
  two bytes (Rule 7.0.5); precedence-clarifying parentheses (Rule 8.0.1); loop bounds held in a `const` local, and
  the chunk and page loops that step their counter in the body written as `while` (Rule 9.5.1); `default: break;`
  in the `ToString(Error)`, `CapacityBytes()` and `ProtectedFrom()` switches, with `-Wswitch-enum` in the test flags
  so a new enumerator still warns (Rule 9.4.2); the `.ipp` include at the end of `at25040b.hpp` marked as deviation
  MD-CPP-009 (Rule 19.0.3). No change in behaviour.
- Follows the driver template's portable ownership: copyright belongs to `The driver-at25040b authors` (`LICENSE`, every
  source header, `driver.yml` `authors`, the docs footer). `tools/check_standalone.py` runs in the *Handbook source
  checks* job and keeps the code and docs about this chip only. `AGENTS.md` follows the template's structure; the
  issue and PR templates ask for the board, MCU and port in one field.
- The port concepts match each hook's address against exactly one `noexcept` signature with
  `detail::ExactHook<Port, Sig>::Take(&Port::Hook)` instead of probing it with a converting `detail::Exactly<T>`:
  no conversion operator is left. A misdeclared hook is still a compile error. `kAlwaysFalse` takes a class and
  the enumeration helpers an enumeration (`ClassType`, `Enumeration`), each with a compile-fail test.
- The bus helper checks every length against its buffer before it sends anything: a READ of 1 to 16 bytes, a WRITE
  of 1 to 8 bytes inside one page, an address up to A8 (`InvalidArgument` otherwise; at compile time for a
  fixed-size span). The transfer buffers are members of the driver object, so the port never sees a caller's or a
  stack buffer (handbook KB-HW-019). The receive buffer is preset to MISO's idle level (0xFF) before each READ, so a
  port that reports success without filling it can never hand back an earlier transfer's bytes. The driver object
  grows by 36 bytes; the hot path is unchanged apart from these comparisons and buffer fills.
- The read chunk, `kReadChunkBytes` = 16, now states its design reason (buffer size, longest CS-low time on a shared
  bus, command overhead, two whole pages); see the Architecture page.
- Include guards instead of `#pragma once` in every header (KB-CPP-019); `uint32_t` casts before every `~`, `<<`,
  `|`, `&` and `^` on register values (KB-CPP20-021); BP1:BP0 through a typed `Field`; the toggle error and the BP
  encoders are free `constexpr` functions with `static_assert`s; `withWriteAccess()` is constrained; enumerator
  checks through `detail::IsEnumerator`; `ToString()` is a switch over every enumerator.
- Template sync to `a4a1056`: `_config/.clang-tidy`, `ci-repo-checks.yml` (handbook source checks pinned), skills.

### Added

- First version of the driver for the Microchip (Atmel) AT25010B, AT25020B and AT25040B SPI serial EEPROMs, written
  on the Kokeb Labs driver template.
- `at25040b::AT25040B<Port>` with the configuration in the constructor (no I/O) and one start-up call,
  `EnsureInitialized()`: the status read until idle and confirmed by a second read, then the write enable latch
  toggled (WREN, WEN = 1, WRDI, WEN = 0) with nothing programmed, so a floating, stuck-low or echoing MISO fails the
  check. `Config::check_write_enable = false` falls back to reading the first 8 bytes twice, for boards with WP tied
  low. `Deinitialize()` (WRDI, WP parked) followed by `EnsureInitialized()` probes again.
- `Read()` of any range in 16-byte transfers, each read twice and compared (`Config::verify_reads`); `Write()` of any
  range split at 8-byte pages, with WREN and a WEN check per page, the write cycle waited for (`Config::write_timeout_us`,
  default 10 ms), the latch checked again and each page read back (`Config::verify_writes`). A range in the
  block-protected part is refused before anything is sent.
- `SetBlockProtection()`, `GetBlockProtection()`, `ReadStatus()`, `GetLastStatusByte()`; `Config::variant` for the
  three sizes, with A8 in the opcode on the AT25040B.
- The optional `GpioSet(CtrlPin::WriteProtect, ...)` port hook: WP is released only for a write, a block-protection
  change and the identity check, and parked after, on success and failure.
- Chip-specific errors from 16: `NoDevice`, `BusNoise`, `WriteProtected`, `BlockProtected`, `WriteMismatch`.
- Host tests over a protocol model of the three parts with the bus and board faults the datasheet's pin descriptions
  allow, every transfer and WP pin write of every operation failed in turn, a libFuzzer target with corpus replay, the
  host and ESP32 examples, three figures drawn from the code. Not verified on hardware; the bench checks are in
  `docs/limitations.md`.

### Changed

- CI runs fewer jobs, from the driver template: the host tests build GCC and Clang in one job; DCO, the org
  standard, Markdown and YAML lint, the handbook rules, template drift and cppcheck are steps of one `Repository
  checks` job (`ci-repo-checks.yml`, replacing `ci-handbook.yml`, `ci-cpp-analysis.yml`, `ci-markdown-lint.yml` and
  `ci-yaml-lint.yml`); cross-compile and host quality use ci-tools' one-job layouts. No check is dropped. The
  workflows have no manual (`workflow_dispatch`) trigger any more.
- Read and write verification is opt-in and chosen per call: `Read()` and `Write()` take a last argument,
  `at25040b::Verify` (`None`, the default, or `ReadBack`). By default `Read()` reads each 16-byte chunk once and
  `Write()` reads no page back, so a call sends only what the datasheet needs plus the status reads that keep it off a
  busy chip. `Verify::ReadBack` restores the double read (`BusNoise`) and the per-page read-back (`WriteMismatch`).
  `Config::verify_reads`, `Config::verify_writes` and their Kconfig options `CONFIG_AT25040B_SKIP_READ_VERIFY` and
  `CONFIG_AT25040B_SKIP_WRITE_VERIFY` are removed. `Config::check_write_enable` stays on by default: it runs once, in
  `EnsureInitialized()`, never on the `Read()` and `Write()` path.
