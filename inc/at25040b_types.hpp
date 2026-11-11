/**
 * @file at25040b_types.hpp
 * @brief Public types of the AT25040B driver: errors, `Result<T>`, variants, block protection and
 * configuration.
 * @copyright Copyright (c) 2026 The driver-at25040b authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is plain data with no platform dependency, so handlers, tests and tools can
 * include it without the driver template. Section and table numbers refer to the AT25010B/020B/040B
 * datasheet, Atmel-8707F (01/2015).
 */
#ifndef AT25040B_TYPES_HPP
#define AT25040B_TYPES_HPP

#include <cstddef>
#include <cstdint>

#include "at25040b_detail.hpp"
#include "at25040b_version.h"

/** @brief The AT25040B driver: everything it declares lives here; internals in `detail`. */
namespace at25040b {

/**
 * @defgroup at25040b_types Driver types
 * @brief Errors, results, variants and configuration.
 * @{
 */

/**
 * @brief Why a call failed. Every call of the driver returns one of these in its `Result`.
 *
 * Values 0 to 9 are the shared base set; chip-specific values start at 16. `[[nodiscard]]` on
 * the type: a function that returns a bare `Error` cannot have it ignored either (KB-CPP20-037).
 */
enum class [[nodiscard]] Error : uint8_t {
  // Base set: the same names and values in every driver of the org (standards/driver.md).
  Ok = 0,               ///< The call succeeded.
  NotInitialized = 1,   ///< `EnsureInitialized()` has not succeeded, or `Deinitialize()` ran since.
  InvalidArgument = 2,  ///< An argument or a `Config` field is out of range; nothing was sent.
  Bus = 3,              ///< A transfer hook (`Transfer()`, `EnsureInitialized()`) returned `false`.
  Timeout = 4,          ///< After the start, the status read busy (0xFF) for longer than
                        ///< `Config::write_timeout_us`: a write cycle that does not end, or the
                        ///< chip was unplugged since and MISO floats high.
  DeviceIdMismatch = 5, ///< At start-up, the bus answers but not like an AT25xxxB: the status is
                        ///< neither idle nor busy (0x05 is MOSI echoed onto MISO), or the write
                        ///< enable latch did not follow WREN and WRDI (MISO stuck low, no chip, or
                        ///< WP held low with `Config::check_write_enable` on).
  Crc = 6,              ///< Not returned by this driver (the chip sends no CRC).
  Busy = 7,             ///< Not returned by this driver: every call waits for a write cycle.
  DeviceFault = 8,      ///< Not returned by this driver (the chip reports no faults).
  Unsupported = 9,      ///< Not returned by this driver.
  // Chip-specific values start at 16; each one is returned by some call (tests/test_errors.cpp).
  NoDevice = 16,       ///< At start-up, the status read 0xFF for longer than
                       ///< `Config::write_timeout_us`: nothing drives MISO (pulled or floating
                       ///< high), so no chip answers.
  BusNoise = 17,       ///< A reply no working chip gives: two reads of the same thing differed, or
                       ///< after the start a status byte was neither idle nor busy.
  WriteProtected = 18, ///< WREN did not set the write enable latch: the WP pin is low (tied low,
                       ///< or the port's WP hook did not release it), or the WREN frame was lost.
                       ///< Nothing was written.
  BlockProtected = 19, ///< The range lies in the block-protected part of the array (BP1, BP0);
                       ///< only the status was read, nothing was written.
  WriteMismatch = 20,  ///< After a write the chip does not hold what was sent: the read-back
                       ///< differed (`Verify::ReadBack`), the write enable latch was still set
                       ///< after the write (the chip ignored it), or BP1 and BP0 did not take.
  Pin = 21, ///< The port's `GpioSet()` could not drive WP (release or park); the name and
            ///< meaning the handbook gives a failed pin hook (KB-CPP-047).
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string such as `"Timeout"`; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  // Every enumerator, and a default for a value that is none (MISRA C++:2023 Rule 9.4.2):
  // -Wswitch-enum flags a new value without a name (KB-CPP-053).
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::NoDevice:
    return "NoDevice";
  case Error::BusNoise:
    return "BusNoise";
  case Error::WriteProtected:
    return "WriteProtected";
  case Error::BlockProtected:
    return "BlockProtected";
  case Error::WriteMismatch:
    return "WriteMismatch";
  case Error::Pin:
    return "Pin";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 *
 * Check `ok()` (or the `bool` conversion) before reading `value`; on failure `value` is `T{}`.
 *
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected that the driver recovered from.
  Info,      ///< Lifecycle events: initialised.
  Debug,     ///< Detail for bring-up.
};

/**
 * @brief Which part of the family is fitted. The chip cannot tell its size, so it is configured.
 *
 * The three parts share the instruction set, the 8-byte page and the status register; only the
 * array size differs, and only the AT25040B uses address bit A8 (datasheet "Description", Table
 * 6-1 note 1).
 */
enum class Variant : uint8_t {
  At25010b = 0, ///< 128 bytes (1 Kbit).
  At25020b = 1, ///< 256 bytes (2 Kbit).
  At25040b = 2, ///< 512 bytes (4 Kbit), A8 in the opcode. The default.
};

/**
 * @brief Block write protection level, BP1:BP0 in the status register (Table 6-4).
 *
 * The protected part is always at the top of the array; the addresses depend on the variant
 * (`ProtectedFrom()`).
 */
enum class BlockProtect : uint8_t {
  None = 0,         ///< BP1:BP0 = 00: nothing protected. The value of a new part.
  UpperQuarter = 1, ///< 01: the top quarter is read-only (0x180 to 0x1FF on the AT25040B).
  UpperHalf = 2,    ///< 10: the top half is read-only (0x100 to 0x1FF on the AT25040B).
  All = 3,          ///< 11: the whole array is read-only.
};

/**
 * @brief Extra verification for one `Read()` or `Write()` call, chosen per call.
 *
 * The datasheet needs neither: `Verify::None`, the default, sends only what it needs, plus the
 * status reads that keep a call off a busy chip. `Verify::ReadBack` costs transfers on the bus and
 * is for a caller that has no CRC or signature of its own over the data.
 */
enum class Verify : uint8_t {
  None = 0,     ///< Each chunk read once; no page read back. The default.
  ReadBack = 1, ///< `Read()`: each chunk read twice and compared (`BusNoise`), twice the READs.
                ///< `Write()`: each page read back and compared (`WriteMismatch`), one READ
                ///< more per page.
};

/** @brief The status register, decoded (Tables 6-2 and 6-3). */
struct Status {
  uint8_t raw{0U};  ///< The byte RDSR returned.
  bool busy{false}; ///< A write cycle runs: every bit reads 1, the other fields mean nothing.
  bool write_enabled{false}; ///< WEN: the write enable latch is set.
  BlockProtect block_protect{///< BP1:BP0.
                             BlockProtect::None};
};

/// Bytes per page: a WRITE never crosses an 8-byte page (section 6, "Write Sequence").
inline constexpr std::size_t kPageSizeBytes = 8;
/**
 * @brief Data bytes per READ transfer: `Read()` splits longer requests into transfers of at most
 * this many data bytes.
 *
 * The datasheet sets no limit (a READ continues through the array, section 6, "Read Sequence"), so
 * this is a design choice, sized by what it costs:
 *
 * - The port: `kMaxTransferBytes` (18) is the longest transfer a port must take, and the driver
 *   object holds two buffers of that size (36 bytes, see `detail::EepromBus`). A whole-array chunk
 *   would make that 2 x 514 bytes in every driver object and every port's DMA descriptor.
 * - The bus: CS goes high after each chunk, so a long `Read()` never holds a shared bus for more
 *   than 18 x 8 = 144 SCK cycles: 29 us at the 5 MHz of the full supply range, 7.2 us at 20 MHz.
 * - The overhead: 2 command bytes per 16 data bytes (11 %); halving the chunk to 8 would cost 20 %,
 *   doubling it to 32 saves 5 % of the bus time and doubles the buffers.
 * - `Verify::ReadBack`: two whole pages per chunk, so the compare buffer is 16 bytes.
 *
 * Not a `Config` value: the buffers are sized at compile time from it, and no setting of it changes
 * what the chip does. A port with a smaller transfer limit cannot use this driver as it is.
 */
inline constexpr std::size_t kReadChunkBytes = 16;
/// Longest transfer the driver asks of the port: opcode, address byte and `kReadChunkBytes`.
inline constexpr std::size_t kMaxTransferBytes = 2 + kReadChunkBytes;
/// Longest self-timed write cycle t_WC, in microseconds (Table 4-3, maximum, every supply).
inline constexpr uint32_t kWriteCycleMaxUs = 5000;
/// Time between two status reads while the driver waits for a write cycle, in microseconds.
inline constexpr uint32_t kPollIntervalUs = 100;
/// Longest `Config::write_timeout_us` the driver accepts, in microseconds (one second).
inline constexpr uint32_t kWriteTimeoutMaxUs = 1000000;
/// Bytes from address 0 that `EnsureInitialized()` reads twice when `check_write_enable` is off.
inline constexpr std::size_t kIdentityReadBytes = 8;

static_assert((kReadChunkBytes >= kPageSizeBytes) && ((kReadChunkBytes % kPageSizeBytes) == 0U),
              "a chunk is a whole number of pages, so a page read back fits in one READ");
static_assert(kIdentityReadBytes <= kReadChunkBytes, "the identity read is one READ");
static_assert(((kWriteCycleMaxUs % kPollIntervalUs) == 0U) &&
                  ((kWriteTimeoutMaxUs % kPollIntervalUs) == 0U),
              "the status poll counts whole intervals up to the timeout");

/**
 * @brief Array size of a variant.
 * @param variant The part.
 * @return 128, 256 or 512; 0 for a value that is not an enumerator.
 */
constexpr std::size_t CapacityBytes(Variant variant) noexcept {
  switch (variant) {
  case Variant::At25010b:
    return 128U;
  case Variant::At25020b:
    return 256U;
  case Variant::At25040b:
    return 512U;
  default:
    break;
  }
  return 0U;
}

/**
 * @brief First protected address of a protection level (Table 6-4).
 * @param variant The part.
 * @param level The level.
 * @return The lowest read-only address; `CapacityBytes(variant)` when nothing is protected.
 */
constexpr std::size_t ProtectedFrom(Variant variant, BlockProtect level) noexcept {
  const std::size_t size = CapacityBytes(variant);
  switch (level) {
  case BlockProtect::None:
    return size;
  case BlockProtect::UpperQuarter:
    return size - (size / 4U);
  case BlockProtect::UpperHalf:
    return size / 2U;
  case BlockProtect::All:
    return 0U;
  default:
    break;
  }
  return 0U;
}

// Table 6-4, the AT25040B column: 180-1FF, 100-1FF, 000-1FF protected.
static_assert(ProtectedFrom(Variant::At25040b, BlockProtect::UpperQuarter) == 0x180U);
static_assert(ProtectedFrom(Variant::At25040b, BlockProtect::UpperHalf) == 0x100U);
static_assert(ProtectedFrom(Variant::At25010b, BlockProtect::UpperQuarter) == 0x60U);
static_assert(ProtectedFrom(Variant::At25020b, BlockProtect::UpperHalf) == 0x80U);
static_assert(CapacityBytes(Variant::At25040b) == (static_cast<std::size_t>(0x1FFU) + 1U));

namespace detail {
/// Last enumerator of `Variant` (detail::IsEnumerator).
template <>
struct EnumLast<Variant> {
  static constexpr Variant kValue = Variant::At25040b; ///< The last enumerator.
};
/// Last enumerator of `BlockProtect`: BP1:BP0 hold 0 to 3, all four are levels (Table 6-4).
template <>
struct EnumLast<BlockProtect> {
  static constexpr BlockProtect kValue = BlockProtect::All; ///< The last enumerator.
};
/// Last enumerator of `Verify`.
template <>
struct EnumLast<Verify> {
  static constexpr Verify kValue = Verify::ReadBack; ///< The last enumerator.
};
} // namespace detail

// Every value of the two-bit BP field is an enumerator, so decoding it needs no check
// (KB-CPP20-025).
static_assert(detail::ToUnderlying(BlockProtect::All) == 3U);

/**
 * @brief What the driver is told about the board. The chip itself has nothing to configure: its
 * only setting, the block protection, is non-volatile and set with `SetBlockProtection()`.
 *
 * `Config{}` is an AT25040B with WP driven by the port or tied high and the start-up identity
 * check on. The read and write verification is not configured here: each `Read()` and `Write()`
 * call chooses it (`Verify`).
 */
struct Config {
  Variant variant{Variant::At25040b}; ///< The fitted part; every address is checked against it.
  /// Identity check at start-up: set and clear the write enable latch (WREN, RDSR, WRDI, RDSR) and
  /// demand that WEN follows. Nothing is programmed. On by default because it runs once, in
  /// `EnsureInitialized()`, and never on the `Read()` and `Write()` path, and it is what tells a
  /// dead bus (MISO stuck low reads 0x00 like an idle chip) from a chip. Turn it off only for a
  /// board whose WP pin is tied low (read-only use): the check then reads the first
  /// `kIdentityReadBytes` twice instead, which a dead bus with MISO stuck low still passes.
  bool check_write_enable{true};
  /// Longest wait for a write cycle to end, in microseconds; at least `kWriteCycleMaxUs`.
  uint32_t write_timeout_us{2 * kWriteCycleMaxUs};
};

/**
 * @brief Check a configuration without touching the chip.
 * @param config The configuration.
 * @return `Error::Ok`, or `Error::InvalidArgument` when the variant is not an enumerator or the
 *         timeout is below `kWriteCycleMaxUs` or above `kWriteTimeoutMaxUs`.
 */
constexpr Error Validate(const Config& config) noexcept {
  if (!detail::IsEnumerator(config.variant)) {
    return Error::InvalidArgument;
  }
  if ((config.write_timeout_us < kWriteCycleMaxUs) ||
      (config.write_timeout_us > kWriteTimeoutMaxUs)) {
    return Error::InvalidArgument;
  }
  return Error::Ok;
}

static_assert(Validate(Config{}) == Error::Ok, "the default configuration is valid");

/** @return The driver version, e.g. `"0.1.0"`, from the generated `at25040b_version.h`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return AT25040B_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return AT25040B_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return AT25040B_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return AT25040B_VERSION_PATCH;
}

/** @} */

} // namespace at25040b

#endif // AT25040B_TYPES_HPP
