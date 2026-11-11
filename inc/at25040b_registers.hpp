/**
 * @file at25040b_registers.hpp
 * @brief Instruction opcodes and status register bits of the AT25010B, AT25020B and AT25040B.
 * @copyright Copyright (c) 2026 The driver-at25040b authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Section and table numbers refer to the AT25010B/020B/040B datasheet, Atmel-8707F (01/2015). The
 * chip has one register, the status register; everything else is the memory array.
 */
#ifndef AT25040B_REGISTERS_HPP
#define AT25040B_REGISTERS_HPP

#include <cstdint>

#include "at25040b_detail.hpp"

/** @brief Opcodes and bit masks of the chip, as `constexpr` values. */
namespace at25040b::reg {

using detail::operator""_u8;

// Instructions (Table 6-1). Every opcode is `0000 xyyy`; bit 3 ("X") is a don't-care for the first
// four and carries address bit A8 of READ and WRITE on the AT25040B (Table 6-1, note 1).
inline constexpr uint8_t kWren = 0x06;  ///< WREN: set the write enable latch.
inline constexpr uint8_t kWrdi = 0x04;  ///< WRDI: reset the write enable latch.
inline constexpr uint8_t kRdsr = 0x05;  ///< RDSR: read the status register.
inline constexpr uint8_t kWrsr = 0x01;  ///< WRSR: write the status register (BP1, BP0).
inline constexpr uint8_t kRead = 0x03;  ///< READ: read data from the array.
inline constexpr uint8_t kWrite = 0x02; ///< WRITE: write data to the array.

/// Bit of a READ or WRITE opcode that carries address bit A8 (AT25040B only, Table 6-1 note 1).
inline constexpr uint8_t kOpcodeA8 = 0x08;
/// Address bit that goes into the opcode instead of the address byte.
inline constexpr uint16_t kAddressA8 = 0x100;
/// Address bits A7 to A0: the address byte of READ and WRITE (Figures 7-6 and 7-7).
inline constexpr uint16_t kAddressLowMask = 0xFF;
/// The highest address any part of the family decodes (A8 to A0, AT25040B).
inline constexpr uint16_t kAddressMax = 0x1FF;

// Status register (Tables 6-2 and 6-3).
inline constexpr uint8_t kStatusRdy = 0x01;      ///< RDY: 1 while a write cycle runs.
inline constexpr uint8_t kStatusWen = 0x02;      ///< WEN: 1 when the write enable latch is set.
inline constexpr uint8_t kStatusBp0 = 0x04;      ///< BP0: block protection, low bit.
inline constexpr uint8_t kStatusBp1 = 0x08;      ///< BP1: block protection, high bit.
inline constexpr uint8_t kStatusBpMask = 0x0C;   ///< BP1 and BP0 together.
inline constexpr uint8_t kStatusBpShift = 2;     ///< Position of BP0.
inline constexpr uint8_t kStatusUnused = 0xF0;   ///< Bits 7 to 4: 0 when no write cycle runs.
inline constexpr uint8_t kStatusBusyByte = 0xFF; ///< Every bit reads 1 during a write cycle.

/// BP1:BP0 as a field of the status byte (Table 6-4): two bits from bit 2.
using StatusBp = detail::Field<uint8_t, kStatusBpShift, 2U>;
static_assert(StatusBp::kMask == kStatusBpMask, "BP1:BP0 are bits 3 and 2 (Table 6-2)");
static_assert((static_cast<uint32_t>(kStatusBp0) | static_cast<uint32_t>(kStatusBp1)) ==
              static_cast<uint32_t>(kStatusBpMask));

/**
 * @brief True for a status byte an idle chip can return: bits 7 to 4 clear and RDY clear
 * (Table 6-3). A busy chip returns `kStatusBusyByte` instead; anything else is not a status.
 * @param status The byte RDSR returned.
 * @return Whether it is an idle status.
 */
constexpr bool IsIdleStatus(uint8_t status) noexcept {
  return (static_cast<uint32_t>(status) &
          (static_cast<uint32_t>(kStatusUnused) | static_cast<uint32_t>(kStatusRdy))) == 0U;
}

static_assert(IsIdleStatus(0x00_u8) && IsIdleStatus(0x0E_u8),
              "WEN and BP1:BP0 may be set when idle");
static_assert(!IsIdleStatus(kStatusBusyByte) && !IsIdleStatus(kRdsr) && !IsIdleStatus(0x10_u8),
              "busy, an echoed RDSR opcode and a set unused bit are not idle");

} // namespace at25040b::reg

#endif // AT25040B_REGISTERS_HPP
