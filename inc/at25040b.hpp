/**
 * @file at25040b.hpp
 * @brief AT25040B driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-at25040b authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `at25040b::AT25040B<Port>` drives one AT25010B, AT25020B or AT25040B SPI serial EEPROM through
 * a platform port you write (see `at25040b_spi_interface.hpp`). The driver has no platform code,
 * allocates nothing, throws nothing, takes no lock and is not thread-safe: give each object one
 * owner, or guard it with one mutex. Section and table numbers refer to the datasheet,
 * Atmel-8707F (01/2015).
 */
#ifndef AT25040B_HPP
#define AT25040B_HPP

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "at25040b_detail.hpp"
#include "at25040b_registers.hpp"
#include "at25040b_spi_interface.hpp"
#include "at25040b_types.hpp"
#include "at25040b_version.h"

namespace at25040b {

namespace detail {

// Optional hooks: a `Declares*` concept only tests the name, the `Has*` concept the exact
// signature, `noexcept` and the result type; the driver rejects a hook that is declared but does
// not match (KB-CPP20-007).

/// The port declares something called `GpioSet` (used to reject a misdeclared hook).
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::WriteProtect, GpioSignal::ACTIVE); };

/// The port drives the WP pin: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };

/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1U); };

/// The port waits: `void DelayUs(uint32_t us) noexcept`. A narrower parameter would truncate long
/// waits.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };

/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };

/// The port takes log lines: `void Log(LogLevel, const char* message) noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// Wait through the port. A port without `DelayUs()` fails to compile at the first call that needs
/// a wait: the write cycle time is a requirement, never skipped.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (HasDelayUs<Port>) {
    port.DelayUs(us);
  } else {
    static_assert(kAlwaysFalse<Port>,
                  "at25040b: the port must implement `void DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`. The driver never formats text.
template <typename Port>
void Log([[maybe_unused]] Port& port, [[maybe_unused]] LogLevel level,
         [[maybe_unused]] const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  }
}

/// A step `withWriteAccess()` runs with WP released: callable with no argument, returning
/// `Result<void>` (KB-CPP20-005).
template <typename Op>
concept WriteAccessOp =
    std::invocable<Op&> && std::same_as<std::invoke_result_t<Op&>, Result<void>>;

/// Decode BP1:BP0 of a status byte. Every two-bit value is a `BlockProtect` enumerator.
constexpr BlockProtect DecodeBlockProtect(uint8_t status) noexcept {
  return static_cast<BlockProtect>(reg::StatusBp::Extract(status));
}

/// BP1:BP0 of `level` in a status byte, for WRSR (Table 6-4); bit 7 and the others stay 0.
constexpr uint8_t EncodeBlockProtect(BlockProtect level) noexcept {
  return reg::StatusBp::Insert(0_u8, static_cast<uint32_t>(ToUnderlying(level)));
}

/// The error of a WREN/WRDI toggle check whose status read `got` instead of `expected`: only WEN
/// wrong means the latch did not follow (no chip, MISO stuck, WP low); anything else changed, or a
/// byte that is no status at all, means the replies are not stable.
constexpr Error ToggleError(uint8_t got, uint8_t expected) noexcept {
  const bool only_wen =
      reg::IsIdleStatus(got) &&
      ((static_cast<uint32_t>(got) ^ static_cast<uint32_t>(expected)) == reg::kStatusWen);
  return only_wen ? Error::DeviceIdMismatch : Error::BusNoise;
}

static_assert((DecodeBlockProtect(0x0C_u8) == BlockProtect::All) &&
              (DecodeBlockProtect(0x06_u8) == BlockProtect::UpperQuarter));
static_assert((EncodeBlockProtect(BlockProtect::UpperHalf) == reg::kStatusBp1) &&
              (DecodeBlockProtect(EncodeBlockProtect(BlockProtect::UpperQuarter)) ==
               BlockProtect::UpperQuarter));
static_assert((ToggleError(0x00_u8, reg::kStatusWen) == Error::DeviceIdMismatch) &&
              (ToggleError(0x04_u8, reg::kStatusWen) == Error::BusNoise) &&
              (ToggleError(reg::kStatusBusyByte, 0x00_u8) == Error::BusNoise));

} // namespace detail

/**
 * @defgroup at25040b_driver AT25040B driver
 * @brief The driver class.
 * @{
 */

// clang-format off: the hook table below must keep one row per line.
/**
 * @brief Driver for one AT25010B, AT25020B or AT25040B on a platform port.
 *
 * Lifecycle: construct with the configuration (no I/O), then `EnsureInitialized()`, the one way
 * to start (bus up, identity check), use, optionally `Deinitialize()`. Every call that touches the
 * chip returns `Error::NotInitialized` until `EnsureInitialized()` has succeeded; no call starts
 * the chip as a side effect, so a probe happens only when you ask for it.
 *
 * Every call that reads or writes the array first reads the status and, if a write cycle runs,
 * waits for it (at most `Config::write_timeout_us`): it never sends an instruction the chip would
 * ignore. Every write and every block-protection change waits for its own write cycle before it
 * returns, so the next call finds the chip ready.
 *
 * Optional port hooks, found at compile time:
 *
 * | Hook | Used for | Without it |
 * | --- | --- | --- |
 * | `bool GpioSet(CtrlPin, GpioSignal) noexcept` | WP: released (high) only while a write, a block-protection change or the identity check runs; parked (low) after, on success and failure | WP must be tied high for writes (or low, read-only: set `Config::check_write_enable = false`) |
 * | `void Log(LogLevel, const char* message) noexcept` | One line per lifecycle event or failure | Nothing is logged |
 *
 * A hook declared with the wrong signature (for example `void GpioSet(...)`) is a compile error.
 * The driver never drives HOLD: tie it high (a HOLD strapped to CS makes every status read fail).
 *
 * @tparam Port Your class derived from `SpiInterface<Port>`.
 */
// clang-format on
template <SpiPort Port>
class AT25040B {
  static_assert(!detail::DeclaresGpioSet<Port> || detail::HasGpioSet<Port>,
                "at25040b: the optional GpioSet hook must be `bool GpioSet(CtrlPin, GpioSignal) "
                "noexcept`, so that a failed pin write is reported");
  static_assert(
      !detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
      "at25040b: the DelayUs hook must be `void DelayUs(uint32_t us) noexcept`, so that no "
      "wait is truncated");
  static_assert(!detail::DeclaresLog<Port> || detail::HasLog<Port>,
                "at25040b: the optional Log hook must be `void Log(LogLevel, const char* message) "
                "noexcept`");

public:
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O, cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config Checked by `EnsureInitialized()`, before any I/O.
   */
  explicit AT25040B(Port& port, const Config& config = {}) noexcept : bus_(port), config_(config) {}

  /** @brief One driver object per chip. */
  AT25040B(const AT25040B&) = delete;
  /** @brief One driver object per chip. */
  AT25040B& operator=(const AT25040B&) = delete;

  /// @name Lifecycle
  /// @{

  /**
   * @brief Check that an AT25xxxB answers, once: the only way to initialise the driver.
   *
   * Brings the bus up, parks WP (with `GpioSet()`), then checks the identity, strongest test
   * first: the status is read until it is not busy (a write cycle may still run after an MCU
   * reset), then once more, and both reads must be the same idle status (bits 7 to 4 and RDY
   * clear). With `Config::check_write_enable` (the default) it then releases WP, clears the latch
   * if it was set (WRDI), sends WREN and reads WEN = 1, sends WRDI and reads WEN = 0, and parks WP:
   * a bus whose MISO floats or is stuck cannot pass this, and nothing is programmed. Without it,
   * the first `kIdentityReadBytes` are read twice and must be equal. After it succeeds, later calls
   * return `Ok` at once with no I/O; after it fails, the next call starts again from the beginning.
   *
   * @return `Ok`, `InvalidArgument` (the configuration fails `Validate()`; no I/O), `Bus`, `Pin`
   *         (the WP hook failed), `NoDevice` (status 0xFF throughout `Config::write_timeout_us`),
   *         `DeviceIdMismatch` or `BusNoise`.
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `EnsureInitialized()` and before `Deinitialize()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Leave the chip safe and mark the driver uninitialised: WRDI, then WP parked.
   *
   * The driver is uninitialised afterwards even if a step failed; the next `EnsureInitialized()`
   * runs the identity check again, which is how a handler probes the chip again. On an
   * uninitialised driver it does nothing and returns `Ok`.
   *
   * @return `Ok`, `Bus` (WRDI failed) or `Pin` (WP could not be parked; reported when both steps
   *         fail, since WP left released is the state the caller must act on).
   */
  Result<void> Deinitialize() noexcept;

  /// @}
  /// @name Configuration
  /// @{

  /**
   * @brief Replace the configuration. Never any I/O: the chip has no setting to write. The
   * variant and the checks apply from the next call; `check_write_enable` from the next start.
   * @param config Checked with `Validate()`.
   * @return `Ok` or `InvalidArgument` (the stored configuration is unchanged).
   */
  Result<void> ApplyConfig(const Config& config) noexcept;

  /** @return The configuration last applied or stored. No I/O. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief No reference into a temporary driver: it would dangle. */
  void GetConfig() const&& = delete;

  /** @return The array size of the configured variant, in bytes. No I/O. */
  [[nodiscard]] std::size_t GetCapacityBytes() const noexcept {
    return CapacityBytes(config_.variant);
  }

  /// @}
  /// @name Memory
  /// @{

  /**
   * @brief Read `out.size()` bytes from `address` (sequential READ, section 6, "Read Sequence").
   *
   * Waits for a running write cycle first (two status reads on an idle chip: the idle status is
   * confirmed). The read is split into transfers of at most `kReadChunkBytes` data bytes. With
   * `Verify::None` each is read once: 2 + ceil(n / 16) transfers for n bytes. With
   * `Verify::ReadBack` each is read twice and compared (`BusNoise`): 2 + 2 x ceil(n / 16)
   * transfers, twice the bus time. The range never wraps: the chip would roll over to address 0,
   * the driver refuses.
   *
   * @param address First address, below `GetCapacityBytes()`.
   * @param out Where the bytes go: at least 1 byte, `address + out.size()` at most
   *        `GetCapacityBytes()`. A `std::array` or a C array converts to it.
   * @param verify `Verify::None` (the default) or `Verify::ReadBack`.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (empty or out-of-range, or `verify` not an
   *         enumerator; nothing sent, `out` untouched), `Bus`, `Timeout` or `BusNoise`. After a
   *         failure past the argument checks, every byte of `out` is 0: no partial data is left
   *         behind.
   */
  Result<void> Read(uint16_t address, std::span<uint8_t> out,
                    Verify verify = Verify::None) noexcept;

  /**
   * @brief Write `data` from `address`: one WRITE and one write cycle per 8-byte page.
   *
   * Reads the status first and refuses a range that overlaps the block-protected part
   * (`BlockProtected`) before sending anything else. Then, with WP released: per page WREN, a
   * status read that must show WEN = 1 (`WriteProtected` otherwise), the WRITE, a wait for the
   * write cycle (`Timeout` after `Config::write_timeout_us`), a status that must show WEN = 0 again
   * (`WriteMismatch` when the chip ignored the WRITE; this reuses the last poll, no extra transfer)
   * and, with `Verify::ReadBack`, a read back (`WriteMismatch`). WP is parked after, on success and
   * failure; a failure after WREN sends WRDI, so the latch is not left set.
   *
   * Cost with `Verify::None`: 2 status reads, then per page WREN, RDSR, WRITE and the status polls
   * of the write cycle (at least 2: the idle status is confirmed), so 2 + 5 x pages transfers plus
   * one RDSR per extra 100 µs poll. `Verify::ReadBack` adds one READ per page.
   *
   * After a failure, the pages before the failing one hold the new data (and were read back, with
   * `Verify::ReadBack`); the failing page may hold old, new or, after a failed WRITE transfer,
   * partly written data; nothing after it and nothing outside the range is touched.
   *
   * @param address First address, below `GetCapacityBytes()`.
   * @param data The bytes to write: at least 1, `address + data.size()` at most
   *        `GetCapacityBytes()`. A `std::array` or a C array converts to it.
   * @param verify `Verify::None` (the default) or `Verify::ReadBack`.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (nothing sent; also for a `verify` that is
   *         not an enumerator), `Bus`, `Pin` (the WP hook failed), `Timeout`, `BusNoise`,
   *         `BlockProtected`, `WriteProtected` or `WriteMismatch`.
   */
  Result<void> Write(uint16_t address, std::span<const uint8_t> data,
                     Verify verify = Verify::None) noexcept;

  /// @}
  /// @name Status and block protection
  /// @{

  /**
   * @brief One status read (RDSR), decoded. Does not wait: during a write cycle it returns
   * `busy` = true.
   * @return The status, or `NotInitialized`, `Bus`, or `BusNoise` (a byte that is neither idle
   *         nor busy).
   */
  Result<Status> ReadStatus() noexcept;

  /**
   * @brief The block protection level, from the status (Table 6-4). Waits for a running write
   * cycle first.
   * @return The level, or `NotInitialized`, `Bus`, `Timeout` or `BusNoise`.
   */
  Result<BlockProtect> GetBlockProtection() noexcept;

  /**
   * @brief Set BP1:BP0 (WRSR, section 6, "Write Status Register").
   *
   * The bits are non-volatile and take a write cycle like a WRITE, so the call waits for it and
   * then reads the level back. When the chip already holds `level`, nothing is written. WP is
   * released for the call and parked after, as for `Write()`.
   *
   * @param level The new level.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (not an enumerator; nothing sent), `Bus`,
   *         `Pin` (the WP hook failed), `Timeout`, `BusNoise`, `WriteProtected` or `WriteMismatch`
   *         (the level read back is not `level`).
   */
  Result<void> SetBlockProtection(BlockProtect level) noexcept;

  /**
   * @brief The last status byte the driver read, by any call, for logs: `0xFF` before the first.
   * No I/O (handbook KB-CPP-129).
   * @return After a failed `EnsureInitialized()`, 0xFF means MISO floats high (or a write cycle
   *         never ended), 0x05 an echo of the RDSR opcode, 0x00 an idle chip or MISO stuck low.
   */
  [[nodiscard]] uint8_t GetLastStatusByte() const noexcept {
    return last_status_;
  }

  /**
   * @brief Read the status register once: one RDSR transfer, no wait, no retry, no decoding and no
   * check of the byte (handbook KB-CPP-129).
   *
   * For bring-up and diagnostics. RDSR has no side effect on the chip (section 6, "Read Status
   * Register") and works during a write cycle (it then reads 0xFF). The byte is also kept for
   * `GetLastStatusByte()`.
   *
   * @return The byte as it came off MISO, or `NotInitialized` or `Bus`.
   */
  Result<uint8_t> ReadRawStatus() noexcept;

  /// @}

private:
  Result<void> initialize() noexcept;
  Result<void> checkWriteEnableToggle(uint8_t idle) noexcept;
  Result<void> checkStableHead() noexcept;
  Result<void> expectAfter(uint8_t opcode, uint8_t expected) noexcept;
  Result<uint8_t> readStatusByte() noexcept;
  Result<uint8_t> waitIdle(bool started) noexcept;
  Result<void> readRange(uint16_t address, std::span<uint8_t> out, Verify verify) noexcept;
  Result<void> writeRange(uint16_t address, std::span<const uint8_t> data, Verify verify) noexcept;
  Result<void> writePage(uint16_t address, std::span<const uint8_t> data, Verify verify) noexcept;
  Result<void> writeStatusRegister(BlockProtect level) noexcept;
  Result<void> enableWrite() noexcept;
  Result<uint8_t> finishWriteCycle() noexcept;
  void disableWrite() noexcept;
  Result<void> releaseWriteProtect() noexcept;
  Result<void> parkWriteProtect() noexcept;
  template <detail::WriteAccessOp Op>
  Result<void> withWriteAccess(Op op) noexcept;
  [[nodiscard]] bool inRange(uint16_t address, std::size_t length) const noexcept;
  Result<void> fail(Error error, const char* message) noexcept;

  detail::EepromBus<Port> bus_;
  Config config_{};
  uint8_t last_status_{reg::kStatusBusyByte};
  bool initialized_{false};
};

/** @} */

} // namespace at25040b

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/at25040b.ipp" // IWYU pragma: export

#endif // AT25040B_HPP
