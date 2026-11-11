/**
 * @file test_model.cpp
 * @brief The chip model's map against the driver's, at compile time (handbook KB-TST-030).
 *
 * tests/mock_at25040b.hpp is written from the datasheet without the driver's headers, so a constant
 * the driver gets wrong is not copied into the model. These checks make the two agree, or the build
 * fails: a difference is a mistake in one of them, found against the datasheet section the model
 * names.
 */
// The model first, on its own: it must compile without any driver header (KB-TST-030).
#include "mock_at25040b.hpp"

#include <cstddef>
#include <cstdint>

#include "at25040b.hpp"
#include "test_harness.hpp"

namespace {

namespace chip = at25040b_test::chip;
namespace reg = at25040b::reg;
using at25040b::BlockProtect;
using at25040b::Variant;

// --8<-- [start:cross-check]
// Instruction set (Table 6-1).
static_assert(chip::kWren == reg::kWren && chip::kWrdi == reg::kWrdi && chip::kRdsr == reg::kRdsr &&
              chip::kWrsr == reg::kWrsr && chip::kRead == reg::kRead &&
              chip::kWrite == reg::kWrite && chip::kOpcodeA8 == reg::kOpcodeA8);
// Status register (Tables 6-2 and 6-3).
static_assert(chip::kStatusRdy == reg::kStatusRdy && chip::kStatusWen == reg::kStatusWen &&
              chip::kStatusBp0 == reg::kStatusBp0 && chip::kStatusBp1 == reg::kStatusBp1 &&
              chip::kStatusBpShift == reg::kStatusBpShift &&
              chip::kStatusBusy == reg::kStatusBusyByte);
// Organisation and timing ("Features", Table 4-3).
static_assert(chip::kPageBytes == at25040b::kPageSizeBytes &&
              chip::kWriteCycleMaxUs == at25040b::kWriteCycleMaxUs);

constexpr bool SameSize(chip::Part part, Variant variant) {
  return chip::kArrayBytes.at(static_cast<std::size_t>(part)) == at25040b::CapacityBytes(variant);
}
static_assert(SameSize(chip::Part::At25010b, Variant::At25010b) &&
              SameSize(chip::Part::At25020b, Variant::At25020b) &&
              SameSize(chip::Part::At25040b, Variant::At25040b));

// Block protection (Table 6-4): every part, every level.
constexpr bool SameProtection(chip::Part part, Variant variant) {
  const auto& row = chip::kProtectedFrom.at(static_cast<std::size_t>(part));
  for (uint32_t level = 0U; level < 4U; ++level) {
    if (row.at(level) != at25040b::ProtectedFrom(variant, static_cast<BlockProtect>(level))) {
      return false;
    }
  }
  return true;
}
static_assert(SameProtection(chip::Part::At25010b, Variant::At25010b) &&
              SameProtection(chip::Part::At25020b, Variant::At25020b) &&
              SameProtection(chip::Part::At25040b, Variant::At25040b));
// SPI limits per supply band (Table 4-3).
constexpr bool SameBand(const chip::SpiBand& band) {
  const at25040b::BusTiming t = at25040b::BusTimingAt(band.min_mv);
  return t.min_supply_mv == band.min_mv && t.max_clock_hz == band.f_sck_max_hz &&
         t.min_cs_high_ns == band.t_cs_min_ns && t.cs_setup_ns == band.t_css_min_ns &&
         t.cs_hold_ns == band.t_csh_min_ns && t.data_setup_ns == band.t_su_min_ns &&
         t.data_hold_ns == band.t_h_min_ns;
}
static_assert(SameBand(chip::kSpiBands[0]) && SameBand(chip::kSpiBands[1]) &&
              SameBand(chip::kSpiBands[2]));
static_assert(chip::kSpiBands[2].f_sck_max_hz == at25040b::kMaxClockHz &&
              chip::kSpiBands[2].t_cs_min_ns == at25040b::kMinCsHighNs &&
              chip::kSpiBands[2].t_css_min_ns == at25040b::kCsSetupNs &&
              chip::kSpiBands[2].t_csh_min_ns == at25040b::kCsHoldNs &&
              chip::kSpiBands[2].t_su_min_ns == at25040b::kDataSetupNs &&
              chip::kSpiBands[2].t_h_min_ns == at25040b::kDataHoldNs);
// --8<-- [end:cross-check]

} // namespace

TEST_CASE("The model's status byte decodes with the driver's status helpers") {
  at25040b_test::At25040bModel model;
  for (uint8_t bp = 0U; bp < 4U; ++bp) {
    model.bp = bp;
    for (const bool wen : {false, true}) {
      model.wen = wen;
      const uint8_t status = model.StatusByte();
      CHECK(reg::IsIdleStatus(status));
      CHECK_EQ(static_cast<uint32_t>(at25040b::detail::DecodeBlockProtect(status)),
               static_cast<uint32_t>(bp));
    }
  }
  model.busy_us = 1;
  CHECK_EQ(model.StatusByte(), reg::kStatusBusyByte);
}
