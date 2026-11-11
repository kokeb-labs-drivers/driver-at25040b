// clang-format off: the hook table below must keep one row per line.
/**
 * @file at25040b_spi_interface.hpp
 * @brief The SPI port of the AT25040B driver: the bus requirements, the CRTP base, its pin enums,
 * the `SpiPort` concept and the instruction framing.
 * @copyright Copyright (c) 2026 The driver-at25040b authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The driver never calls an SDK. It calls hooks on a class you derive from
 * `at25040b::SpiInterface<Self>`:
 *
 * | Hook | Contract |
 * | --- | --- |
 * | `bool EnsureInitialized() noexcept` | Bring the bus up on the first call; `true` at once afterwards |
 * | `bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept` | One full-duplex transfer, CS low for all `len` bytes and high after; `rx` is `nullptr` when the driver needs no reply |
 * | `void DelayUs(uint32_t us) noexcept` | Wait at least `us` microseconds (write cycles, status polls) |
 *
 * The port moves raw bytes: it never retries, never interprets the frame and never configures the
 * chip. What the chip needs from the bus is below as `constexpr` values (`kSpiMode` to
 * `kContinuousWord`, and `BusTimingAt()` per supply voltage, datasheet section 4.3, Table 4-3, and
 * section 5); a port checks its settings against them with `static_assert`. Every instruction
 * starts with a CS falling edge, so CS must go high between transfers. A transfer is at most
 * `kMaxTransferBytes` long. The optional hooks (`GpioSet` for the WP pin, `Log`) are described in
 * `at25040b.hpp`.
 */
// clang-format on
#ifndef AT25040B_SPI_INTERFACE_HPP
#define AT25040B_SPI_INTERFACE_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

#include "at25040b_detail.hpp"
#include "at25040b_registers.hpp"
#include "at25040b_types.hpp"

namespace at25040b {

/**
 * @name Bus requirements
 * What the AT25010B/020B/040B needs from the SPI bus (datasheet "Features", section 4.3 Table 4-3,
 * section 5). A port and a board configuration check their settings against these with
 * `static_assert` (handbook KB-CPP-123). The scalar limits hold over the whole supply range
 * (1.8 V to 5.5 V); `BusTimingAt()` gives the faster limits of a higher supply.
 * @{
 */
/// SPI mode the driver is tested with: CPOL 0, CPHA 0 ("Features": modes 0 and 3 are supported,
/// the datasheet describes mode 0). The datasheet allows mode 3 as well (not verified on hardware
/// with this driver); nothing else works.
inline constexpr uint8_t kSpiMode = 0;
/// The other SPI mode the chip accepts: CPOL 1, CPHA 1 ("Features").
inline constexpr uint8_t kSpiModeAlternate = 3;
/// Each byte is clocked most significant bit first (section 5, "MSB").
inline constexpr bool kMsbFirst = true;
/// Highest SCK frequency over the whole supply range, f_SCK max at 1.8 V to 5.5 V (Table 4-3).
inline constexpr uint32_t kMaxClockHz = 5'000'000;
/// Shortest time CS stays high between two transfers, t_CS min at 1.8 V to 5.5 V (Table 4-3).
inline constexpr uint32_t kMinCsHighNs = 200;
/// CS low to the first SCK edge, t_CSS min at 1.8 V to 5.5 V (Table 4-3).
inline constexpr uint32_t kCsSetupNs = 200;
/// Last SCK edge to CS high, t_CSH min at 1.8 V to 5.5 V (Table 4-3).
inline constexpr uint32_t kCsHoldNs = 200;
/// SI valid before the sampling SCK edge, t_SU min at 1.8 V to 5.5 V (Table 4-3).
inline constexpr uint32_t kDataSetupNs = 80;
/// SI held after the sampling SCK edge, t_H min at 1.8 V to 5.5 V (Table 4-3).
inline constexpr uint32_t kDataHoldNs = 80;
/// `false`: the chip accepts idle SCK time between the bytes of one transfer (a static design:
/// t_WH and t_WL have no maximum, Table 4-3). CS must stay low for the whole instruction.
inline constexpr bool kContinuousWord = false;

/** @brief The SPI timing limits of one supply range (Table 4-3). */
struct BusTiming {
  uint32_t min_supply_mv{0U};  ///< Lowest VCC of the range; every range ends at 5.5 V.
  uint32_t max_clock_hz{0U};   ///< f_SCK max; 0 for a supply outside 1.8 V to 5.5 V.
  uint32_t min_cs_high_ns{0U}; ///< t_CS min.
  uint32_t cs_setup_ns{0U};    ///< t_CSS min.
  uint32_t cs_hold_ns{0U};     ///< t_CSH min.
  uint32_t data_setup_ns{0U};  ///< t_SU min.
  uint32_t data_hold_ns{0U};   ///< t_H min.
};

/// Lowest supply of the family, in millivolts (Table 4-3).
inline constexpr uint32_t kMinSupplyMv = 1800;
/// Highest supply of the family, in millivolts (Table 4-3).
inline constexpr uint32_t kMaxSupplyMv = 5500;

/**
 * @brief The SPI limits at a supply voltage (Table 4-3): 4.5 V to 5.5 V allows 20 MHz, 2.5 V to
 * 5.5 V 10 MHz, 1.8 V to 5.5 V 5 MHz.
 * @param supply_mv The lowest VCC the board can see, in millivolts.
 * @return The limits; all zero for a supply outside 1.8 V to 5.5 V.
 */
constexpr BusTiming BusTimingAt(uint32_t supply_mv) noexcept {
  if ((supply_mv < kMinSupplyMv) || (supply_mv > kMaxSupplyMv)) {
    return BusTiming{};
  }
  if (supply_mv >= 4500U) {
    return BusTiming{.min_supply_mv = 4500U,
                     .max_clock_hz = 20'000'000U,
                     .min_cs_high_ns = 100U,
                     .cs_setup_ns = 100U,
                     .cs_hold_ns = 100U,
                     .data_setup_ns = 20U,
                     .data_hold_ns = 20U};
  }
  if (supply_mv >= 2500U) {
    return BusTiming{.min_supply_mv = 2500U,
                     .max_clock_hz = 10'000'000U,
                     .min_cs_high_ns = 100U,
                     .cs_setup_ns = 100U,
                     .cs_hold_ns = 100U,
                     .data_setup_ns = 40U,
                     .data_hold_ns = 40U};
  }
  return BusTiming{.min_supply_mv = kMinSupplyMv,
                   .max_clock_hz = kMaxClockHz,
                   .min_cs_high_ns = kMinCsHighNs,
                   .cs_setup_ns = kCsSetupNs,
                   .cs_hold_ns = kCsHoldNs,
                   .data_setup_ns = kDataSetupNs,
                   .data_hold_ns = kDataHoldNs};
}

using detail::operator""_u32;

static_assert((BusTimingAt(3300_u32).max_clock_hz == 10'000'000U) &&
              (BusTimingAt(5000_u32).cs_hold_ns == 100U));
static_assert((BusTimingAt(1800_u32).max_clock_hz == kMaxClockHz) &&
              (BusTimingAt(1799_u32).max_clock_hz == 0U));
static_assert(BusTimingAt(5501_u32).max_clock_hz == 0U);
/** @} */

/**
 * @brief Control pins the port may drive through the optional `GpioSet()` hook.
 *
 * Idle level (handbook KB-HW-018): WP idles ACTIVE (pin low, writes blocked), the safe state of
 * this pin. The port drives it low before it enables the pin's output and before the first
 * transfer, so the chip never sees WP float high; the driver releases it only around a write.
 */
enum class CtrlPin : uint8_t {
  WriteProtect = 0, ///< WP, active low on the package pin. ACTIVE (pin low) blocks every write.
};

/** @brief Logical level of a control pin; the port maps it to the physical polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted (WP: pin high, writes allowed).
  ACTIVE = 1,   ///< The pin function is asserted (WP: pin low, writes blocked).
};

/**
 * @brief CRTP base of the platform SPI port.
 *
 * Derive `class MyPort : public at25040b::SpiInterface<MyPort>` and implement the hooks in this
 * file's description. The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion) and no data; the driver checks your class with the `SpiPort`
 * concept, so a missing or misdeclared hook is a compile error that names it.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class SpiInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface(const SpiInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface& operator=(const SpiInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  SpiInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~SpiInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `SpiInterface<Port>` and implements the required
 * hooks with exactly the types above, `noexcept` and returning `bool`. `DelayUs()` is checked
 * where the driver first waits.
 *
 * Each hook's address is matched with `detail::ExactHook`, which takes a member function of exactly
 * the signature and converts no value: a hook declared with a narrower length (`uint16_t len`) or
 * without `noexcept` does not satisfy the concept, instead of truncating every transfer silently
 * (KB-CPP20-006).
 *
 * @tparam Port The port class.
 */
template <typename Port>
concept SpiPort = std::derived_from<Port, SpiInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, bool(const uint8_t*, uint8_t*, std::size_t) noexcept>::Take(
      &Port::Transfer);
};

namespace detail {

/// READ or WRITE opcode for an address: A8 goes into bit 3 (Table 6-1, note 1). The driver checks
/// the address against the variant first, so on the smaller parts A8 is always 0.
constexpr uint8_t AddressedOpcode(uint8_t opcode, uint16_t address) noexcept {
  return ((static_cast<uint32_t>(address) & static_cast<uint32_t>(reg::kAddressA8)) != 0U)
             ? static_cast<uint8_t>(static_cast<uint32_t>(opcode) |
                                    static_cast<uint32_t>(reg::kOpcodeA8))
             : opcode;
}

/// The address byte A7 to A0 of READ and WRITE (Figures 7-6 and 7-7).
constexpr uint8_t AddressByte(uint16_t address) noexcept {
  return static_cast<uint8_t>(static_cast<uint32_t>(address) &
                              static_cast<uint32_t>(reg::kAddressLowMask));
}

static_assert((AddressedOpcode(reg::kRead, 0x1FF_u16) == 0x0BU) &&
              (AddressedOpcode(reg::kWrite, 0xFF_u16) == reg::kWrite));
static_assert(AddressByte(0x1A5_u16) == 0xA5U);

/// READ and WRITE frames: opcode and address byte before the data.
inline constexpr std::size_t kCommandBytes = 2;
/// Longest transfer the driver asks of the port, and the size of each transfer buffer.
static_assert(kMaxTransferBytes == (kCommandBytes + kReadChunkBytes));
static_assert((kCommandBytes + kPageSizeBytes) <= kMaxTransferBytes,
              "a page write fits the buffer");

/// A READ size the helper takes with its size checked at compile time: 1 to `kReadChunkBytes`.
template <std::size_t N>
concept ReadSize = (N >= 1U) && (N <= kReadChunkBytes);
/// A WRITE size the helper takes with its size checked at compile time: 1 to `kPageSizeBytes`.
template <std::size_t N>
concept WriteSize = (N >= 1U) && (N <= kPageSizeBytes);

/**
 * The instructions of Table 6-1, one transfer each (Figures 7-2 to 7-7).
 *
 * Buffers are spans (KB-CPP20-042): a fixed extent is checked at compile time, a dynamic one before
 * anything is sent (KB-CPP20-043), so no caller can make a transfer overrun the buffers. The port
 * hook is the only pointer-and-length interface, and it only ever sees `tx_` and `rx_`: the
 * transfer buffers are members, so they live where the driver object lives (handbook KB-HW-019:
 * place the driver in memory the bus and its DMA can reach), never on a caller's stack.
 */
template <SpiPort Port>
class EepromBus {
public:
  explicit EepromBus(Port& port) noexcept : port_(port) {}

  Port& GetPort() & noexcept {
    return port_;
  }

  /// A one-byte instruction: WREN or WRDI. `Error::Bus` on a failed transfer.
  Error Instruction(uint8_t opcode) noexcept {
    tx_[0] = opcode;
    return port_.Transfer(tx_.data(), nullptr, 1U) ? Error::Ok : Error::Bus;
  }

  /// RDSR: the status byte follows the opcode (Figure 7-4).
  Error ReadStatus(uint8_t& status) noexcept {
    tx_[0] = reg::kRdsr;
    tx_[1] = 0x00U;
    rx_[1] = reg::kStatusBusyByte;
    if (!port_.Transfer(tx_.data(), rx_.data(), 2U)) {
      return Error::Bus;
    }
    status = rx_[1];
    return Error::Ok;
  }

  /// WRSR: one data byte after the opcode (Figure 7-5).
  Error WriteStatus(uint8_t value) noexcept {
    tx_[0] = reg::kWrsr;
    tx_[1] = value;
    return port_.Transfer(tx_.data(), nullptr, 2U) ? Error::Ok : Error::Bus;
  }

  /// READ of `out.size()` bytes from `address`; the size is checked at compile time.
  template <std::size_t N>
    requires ReadSize<N>
  Error Read(uint16_t address, std::span<uint8_t, N> out) noexcept {
    return Read(address, std::span<uint8_t>{out});
  }

  /// A fixed-size READ longer than `kReadChunkBytes` (or empty) does not compile.
  template <std::size_t N>
    requires((N != std::dynamic_extent) && !ReadSize<N>)
  Error Read(uint16_t address, std::span<uint8_t, N> out) noexcept = delete;

  /// READ of 1 to `kReadChunkBytes` bytes from `address` (Figure 7-6). `InvalidArgument` (nothing
  /// sent) for an empty or longer `out` or an address above A8, `Bus` on a failed transfer.
  Error Read(uint16_t address, std::span<uint8_t> out) noexcept {
    if (out.empty() || (out.size() > kReadChunkBytes) || (address > reg::kAddressMax)) {
      return Error::InvalidArgument;
    }
    tx_.fill(0x00U);
    // MISO idles high: a port that reports success without writing `rx` yields 0xFF, never the
    // bytes of an earlier transfer (KB-CPP-116).
    rx_.fill(reg::kStatusBusyByte);
    tx_[0] = AddressedOpcode(reg::kRead, address);
    tx_[1] = AddressByte(address);
    if (!port_.Transfer(tx_.data(), rx_.data(), kCommandBytes + out.size())) {
      return Error::Bus;
    }
    const std::size_t count = out.size();
    for (std::size_t i = 0U; i < count; ++i) {
      out[i] = rx_[kCommandBytes + i];
    }
    return Error::Ok;
  }

  /// WRITE of `data.size()` bytes from `address`; the size is checked at compile time.
  template <std::size_t N>
    requires WriteSize<N>
  Error Write(uint16_t address, std::span<const uint8_t, N> data) noexcept {
    return Write(address, std::span<const uint8_t>{data});
  }

  /// A fixed-size WRITE longer than a page (or empty) does not compile.
  template <std::size_t N>
    requires((N != std::dynamic_extent) && !WriteSize<N>)
  Error Write(uint16_t address, std::span<const uint8_t, N> data) noexcept = delete;

  /// WRITE of 1 to `kPageSizeBytes` bytes inside one page (Figure 7-7). The write cycle starts when
  /// CS goes high at the end of the transfer. `InvalidArgument` (nothing sent) for an empty range,
  /// one that would cross a page (the chip would wrap to the start of the page) or an address above
  /// A8; `Bus` on a failed transfer.
  Error Write(uint16_t address, std::span<const uint8_t> data) noexcept {
    const std::size_t offset = static_cast<std::size_t>(address) % kPageSizeBytes;
    if (data.empty() || (data.size() > (kPageSizeBytes - offset)) || (address > reg::kAddressMax)) {
      return Error::InvalidArgument;
    }
    tx_[0] = AddressedOpcode(reg::kWrite, address);
    tx_[1] = AddressByte(address);
    const std::size_t count = data.size();
    for (std::size_t i = 0U; i < count; ++i) {
      tx_[kCommandBytes + i] = data[i];
    }
    return port_.Transfer(tx_.data(), nullptr, kCommandBytes + data.size()) ? Error::Ok
                                                                            : Error::Bus;
  }

private:
  Port& port_;
  std::array<uint8_t, kMaxTransferBytes> tx_{};
  std::array<uint8_t, kMaxTransferBytes> rx_{};
};

} // namespace detail

} // namespace at25040b

#endif // AT25040B_SPI_INTERFACE_HPP
