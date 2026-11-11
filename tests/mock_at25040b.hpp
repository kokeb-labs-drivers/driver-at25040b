/**
 * @file mock_at25040b.hpp
 * @brief A protocol model of the AT25010B, AT25020B and AT25040B for the host tests. The mock ports
 * over it are in mock_at25040b_bus.hpp, which includes this file; this one includes no driver
 * header.
 *
 * The model decodes each transfer as the chip does (datasheet Atmel-8707F, section 6): opcodes of
 * Table 6-1 with A8 in bit 3 on the AT25040B and don't-care bits elsewhere, invalid opcodes that
 * leave SO in high impedance, the write enable latch (set by WREN only while WP is high, cleared
 * by WRDI whatever WP does and at the end of every write cycle), sequential READ that rolls over at
 * the top of the array, WRITE whose address counts up in its three low bits only (a ninth byte
 * wraps to the start of the page), a self-timed write cycle that starts when CS goes high and lasts
 * `write_cycle_us` of simulated time, during which every instruction but RDSR is ignored and RDSR
 * reads 0xFF, block protection BP1:BP0 that refuses writes into the protected top of the array and
 * survives `PowerCycle()` like the array.
 *
 * Faults are injected through the public fields; each one is a bus or board condition that the
 * datasheet's pin descriptions allow (section 5: SO in high impedance when deselected or held,
 * WP low inhibiting writes) or that a broken line produces (docs/limitations.md).
 *
 * The model is written from the datasheet on its own (handbook KB-TST-030): it does not include the
 * driver's `at25040b_registers.hpp` or `at25040b_types.hpp`, but carries its own opcodes, status
 * bits, array sizes and protection table below (namespace `chip`, each with its datasheet section
 * and page), so a wrong constant in the driver cannot hide behind the same wrong constant in the
 * model. tests/test_model.cpp checks the two maps against each other at compile time. It has a
 * virtual clock: `Elapse()` runs the self-timed write cycle.
 */
#ifndef AT25040B_MOCK_AT25040B_HPP
#define AT25040B_MOCK_AT25040B_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace at25040b_test {

/// The chip as the datasheet (Atmel-8707F) describes it, independent of the driver's headers.
namespace chip {
// Instruction set (section 6, Table 6-1, page 8): `0000 X???`, X = A8 for READ and WRITE on the
// AT25040B, a don't-care elsewhere.
inline constexpr uint8_t kWren = 0x06;
inline constexpr uint8_t kWrdi = 0x04;
inline constexpr uint8_t kRdsr = 0x05;
inline constexpr uint8_t kWrsr = 0x01;
inline constexpr uint8_t kRead = 0x03;
inline constexpr uint8_t kWrite = 0x02;
inline constexpr uint8_t kOpcodeA8 = 0x08;   // bit 3 of READ and WRITE (Table 6-1, note 1)
inline constexpr uint8_t kOpcodeBase = 0x07; // the bits that name the instruction
inline constexpr uint8_t kOpcodeInvalid = 0xF0;
// Status register (Tables 6-2 and 6-3, page 8).
inline constexpr uint8_t kStatusRdy = 0x01;
inline constexpr uint8_t kStatusWen = 0x02;
inline constexpr uint8_t kStatusBp0 = 0x04;
inline constexpr uint8_t kStatusBp1 = 0x08;
inline constexpr uint32_t kStatusBpShift = 2;
inline constexpr uint8_t kStatusBusy = 0xFF; // every bit reads 1 during a write cycle
// Organisation ("Description" and "Features", page 1): 128, 256, 512 bytes, 8-byte pages.
inline constexpr std::size_t kPageBytes = 8;
inline constexpr std::array<std::size_t, 3> kArrayBytes = {128U, 256U, 512U};
// Table 6-4 (page 9): the first protected address of BP1:BP0 = 0..3, per part (0x80, 0x100, 0x200
// mean nothing protected).
inline constexpr std::array<std::array<uint16_t, 4>, 3> kProtectedFrom = {{
    {0x080U, 0x060U, 0x040U, 0x000U}, // AT25010B: none, 60-7F, 40-7F, 00-7F
    {0x100U, 0x0C0U, 0x080U, 0x000U}, // AT25020B: none, C0-FF, 80-FF, 00-FF
    {0x200U, 0x180U, 0x100U, 0x000U}, // AT25040B: none, 180-1FF, 100-1FF, 000-1FF
}};
// Timing (section 4.3, Table 4-3, page 5): t_WC max, and the SPI limits per supply band.
inline constexpr uint32_t kWriteCycleMaxUs = 5000;
/// One row of Table 4-3: the band starts at `min_mv` and ends at 5.5 V.
struct SpiBand {
  uint32_t min_mv;
  uint32_t f_sck_max_hz;
  uint32_t t_cs_min_ns;  // CS high time
  uint32_t t_css_min_ns; // CS setup
  uint32_t t_csh_min_ns; // CS hold
  uint32_t t_su_min_ns;  // data in setup
  uint32_t t_h_min_ns;   // data in hold
};
inline constexpr std::array<SpiBand, 3> kSpiBands = {{
    {4500U, 20'000'000U, 100U, 100U, 100U, 20U, 20U},
    {2500U, 10'000'000U, 100U, 100U, 100U, 40U, 40U},
    {1800U, 5'000'000U, 200U, 200U, 200U, 80U, 80U},
}};

/// The fitted part: index into the tables above.
enum class Part : uint8_t { At25010b = 0, At25020b = 1, At25040b = 2 };
} // namespace chip

/// What MISO carries, whatever the chip does.
enum class Miso : uint8_t {
  Chip = 0,  ///< The chip drives SO: normal operation.
  FloatHigh, ///< No chip on the CS (module unplugged), MISO pulled or floating high: 0xFF.
  StuckLow,  ///< MISO pulled or shorted low, the chip deaf: every byte reads 0x00, an idle status.
  EchoMosi,  ///< MISO repeats MOSI one byte late (SI shorted to SO, or a loop-back).
  Noise,     ///< Every byte random: a contended or broken MISO.
};

class At25040bModel {
public:
  // --- What the chip is -------------------------------------------------------------------------
  chip::Part variant = chip::Part::At25040b; ///< The fitted part.
  std::array<uint8_t, 512> memory{};         ///< The array; only Capacity() is used.
  uint8_t bp = 0;                            ///< BP1:BP0, non-volatile.
  uint32_t write_cycle_us = 3000;            ///< This part's t_WC (at most 5000).

  // --- Pins and faults --------------------------------------------------------------------------
  bool wp_high = true;            ///< WP pin level as the port drives it (true: writes allowed).
  bool wp_tied_low = false;       ///< The board holds WP low whatever the port does.
  bool hold_tied_to_cs = false;   ///< HOLD strapped to CS: the chip pauses in every frame.
  Miso miso = Miso::Chip;         ///< What MISO carries.
  int ignore_writes = 0;          ///< WRITE frames the chip silently ignores (no cycle, WEN kept).
  int stuck_address = -1;         ///< A cell that keeps its value when programmed; -1 none.
  uint32_t noise_state = 0x1234U; ///< Seed of `Miso::Noise`.

  // --- State and counters (read them, do not set them) ------------------------------------------
  bool wen = false;          ///< The write enable latch.
  uint32_t busy_us = 0;      ///< Time left in the running write cycle; 0 when idle.
  int write_cycles = 0;      ///< Write cycles completed (WRITE and WRSR).
  int wrsr_cycles = 0;       ///< Of those, WRSR cycles.
  int ignored_frames = 0;    ///< Instructions ignored during a write cycle or with a bad opcode.
  int refused_writes = 0;    ///< WRITE or WRSR refused: WEN clear, WP low or protected address.
  std::size_t max_frame = 0; ///< Longest transfer seen.

  At25040bModel() noexcept {
    memory.fill(0xFF); // a new part is erased
  }

  [[nodiscard]] std::size_t Capacity() const noexcept {
    return chip::kArrayBytes.at(static_cast<std::size_t>(variant));
  }
  [[nodiscard]] bool WpHigh() const noexcept {
    return wp_high && !wp_tied_low;
  }
  [[nodiscard]] uint8_t StatusByte() const noexcept {
    if (busy_us > 0) {
      return chip::kStatusBusy;
    }
    return static_cast<uint8_t>((static_cast<uint32_t>(bp) << chip::kStatusBpShift) |
                                (wen ? static_cast<uint32_t>(chip::kStatusWen) : 0U));
  }

  /// A write cycle runs and will program bytes or BP1:BP0 when it ends.
  [[nodiscard]] bool HasPendingWrite() const noexcept {
    return busy_us > 0 && (pending_count_ > 0 || pending_bp_ >= 0);
  }

  /// Power off and on: the latch and a running cycle are lost; the array and BP1:BP0 stay.
  void PowerCycle() noexcept {
    wen = false;
    busy_us = 0;
    pending_count_ = 0;
    pending_bp_ = -1;
  }

  /// Simulated time passes; a write cycle that ends here programs its bytes.
  void Elapse(uint32_t us) noexcept {
    if (busy_us == 0) {
      return;
    }
    if (us < busy_us) {
      busy_us -= us;
      return;
    }
    busy_us = 0;
    finishCycle();
  }

  /// One transfer with CS low for `len` bytes; `rx` may be nullptr.
  void Frame(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    std::array<uint8_t, 64> out{};
    out.fill(0xFF);
    max_frame = len > max_frame ? len : max_frame;
    const std::size_t n = len < out.size() ? len : out.size();
    if (!busMiso(tx, out.data(), n)) {
      if (hold_tied_to_cs) {
        // HOLD low while SCK idles low: the chip pauses, ignores SI and leaves SO in high
        // impedance (section 5, "Hold"). MISO reads its pull-up.
      } else {
        decode(tx, out.data(), n);
      }
    }
    if (rx != nullptr) {
      for (std::size_t i = 0; i < n; ++i) {
        rx[i] = out[i];
      }
    }
  }

private:
  /// The MISO faults that replace the chip; false when the chip itself answers.
  bool busMiso(const uint8_t* tx, uint8_t* out, std::size_t n) noexcept {
    switch (miso) {
    case Miso::Chip:
      return false;
    case Miso::FloatHigh:
      return true; // already 0xFF
    case Miso::StuckLow:
      for (std::size_t i = 0; i < n; ++i) {
        out[i] = 0x00;
      }
      return true;
    case Miso::EchoMosi:
      for (std::size_t i = 1; i < n; ++i) {
        out[i] = tx[i - 1];
      }
      return true;
    case Miso::Noise:
      for (std::size_t i = 0; i < n; ++i) {
        noise_state = (noise_state * 1103515245U) + 12345U;
        out[i] = static_cast<uint8_t>(noise_state >> 16U);
      }
      return true;
    }
    return true;
  }

  [[nodiscard]] uint16_t addressOf(const uint8_t* tx) const noexcept {
    const bool a8 = variant == chip::Part::At25040b && (tx[0] & chip::kOpcodeA8) != 0U;
    const auto address = static_cast<uint16_t>((a8 ? 0x100U : 0U) | tx[1]);
    return static_cast<uint16_t>(address % Capacity());
  }

  [[nodiscard]] bool isProtected(uint16_t address) const noexcept {
    const auto& row = chip::kProtectedFrom.at(static_cast<std::size_t>(variant));
    return address >= row.at(static_cast<std::size_t>(bp & 0x03U));
  }

  void decode(const uint8_t* tx, uint8_t* out, std::size_t n) noexcept {
    if (n == 0) {
      return;
    }
    const uint8_t op = tx[0];
    if ((op & chip::kOpcodeInvalid) != 0U) {
      ignored_frames++; // invalid opcode: SO stays in high impedance
      return;
    }
    const auto base = static_cast<uint8_t>(op & chip::kOpcodeBase); // bit 3 is A8 or a don't-care
    if (busy_us > 0) {
      if (base == chip::kRdsr) {
        statusOut(out, n);
      } else {
        ignored_frames++; // only RDSR works during a write cycle
      }
      return;
    }
    switch (base) {
    case chip::kWren:
      wen = wen || WpHigh(); // WP must be high during WREN
      return;
    case chip::kWrdi:
      wen = false; // independent of WP
      return;
    case chip::kRdsr:
      statusOut(out, n);
      return;
    case chip::kWrsr:
      writeStatus(tx, n);
      return;
    case chip::kRead:
      readOut(tx, out, n);
      return;
    case chip::kWrite:
      writeIn(tx, n);
      return;
    default:
      ignored_frames++;
      return;
    }
  }

  void statusOut(uint8_t* out, std::size_t n) const noexcept {
    for (std::size_t i = 1; i < n; ++i) {
      out[i] = StatusByte();
    }
  }

  void readOut(const uint8_t* tx, uint8_t* out, std::size_t n) const noexcept {
    if (n < 2) {
      return;
    }
    std::size_t address = addressOf(tx);
    for (std::size_t i = 2; i < n; ++i) {
      out[i] = memory[address];
      address = (address + 1) % Capacity(); // rolls over at the top (section 6, "Read Sequence")
    }
  }

  void writeStatus(const uint8_t* tx, std::size_t n) noexcept {
    if (n < 2 || !wen || !WpHigh()) {
      refused_writes++;
      return;
    }
    pending_bp_ = static_cast<int>((static_cast<uint32_t>(tx[1]) >> chip::kStatusBpShift) & 0x03U);
    busy_us = write_cycle_us;
  }

  void writeIn(const uint8_t* tx, std::size_t n) noexcept {
    if (n < 3) {
      return; // no data byte: nothing to program
    }
    const uint16_t address = addressOf(tx);
    if (!wen || !WpHigh() || isProtected(address)) {
      refused_writes++; // the chip returns to standby when CS goes high
      return;
    }
    if (ignore_writes > 0) {
      ignore_writes--; // a WRITE the chip silently did not take
      return;
    }
    const auto page = static_cast<uint16_t>(address & ~0x7U);
    pending_count_ = 0;
    pending_page_ = page;
    pending_mask_ = 0;
    auto offset = static_cast<uint8_t>(address & 0x7U);
    for (std::size_t i = 2; i < n; ++i) {
      pending_[offset] = tx[i]; // only the three low bits count up: a ninth byte wraps
      pending_mask_ = static_cast<uint8_t>(pending_mask_ | (1U << offset));
      offset = static_cast<uint8_t>((offset + 1U) & 0x7U);
      pending_count_++;
    }
    busy_us = write_cycle_us;
  }

  void finishCycle() noexcept {
    if (pending_bp_ >= 0) {
      bp = static_cast<uint8_t>(pending_bp_);
      pending_bp_ = -1;
      wrsr_cycles++;
    }
    for (uint8_t i = 0; i < 8U && pending_count_ > 0; ++i) {
      const auto address = static_cast<int>(pending_page_ + i);
      if ((pending_mask_ & (1U << i)) != 0U && address != stuck_address) {
        memory[static_cast<std::size_t>(address)] = pending_[i];
      }
    }
    pending_count_ = 0;
    pending_mask_ = 0;
    wen = false; // back to write disable at the end of a write cycle
    write_cycles++;
  }

  std::array<uint8_t, 8> pending_{};
  uint16_t pending_page_ = 0;
  uint8_t pending_mask_ = 0;
  int pending_count_ = 0;
  int pending_bp_ = -1;
};

} // namespace at25040b_test

#endif // AT25040B_MOCK_AT25040B_HPP
