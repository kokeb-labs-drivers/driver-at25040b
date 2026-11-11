/**
 * @file fuzz_at25040b.cpp
 * @brief Fuzz target: the driver over a chip model and a bus that the fuzzer controls.
 *
 * Everything the driver receives comes from MISO, so MISO is the untrusted input. The fuzzer's
 * bytes choose a configuration and the `Verify` value of the `Read()` and `Write()` calls, then a
 * sequence of driver calls (a configuration call chooses both again); on every transfer they also
 * choose what the bus does: pass the chip model's reply, fail the transfer, lose it (the chip never
 * sees it and MISO floats), flip bits of the reply or replace it; on every WP pin write, pass or
 * fail it. Between calls they change the board: MISO floating high, stuck low, echoing MOSI or
 * noisy, HOLD strapped to CS, WP tied low, writes the chip silently ignores, a stuck cell, a slow
 * or running write cycle, the block protection, a power cycle.
 *
 * A transfer is "tampered" when the bus, not the chip, made its reply (a lost, flipped or replaced
 * transfer, or any transfer while MISO is not the chip's). After every call the target checks the
 * driver's promises (docs/contract.md), and aborts, naming the line, when one breaks:
 *
 * - a failed call carries an `Error` the enum defines, and a value of `T{}`;
 * - before `EnsureInitialized()` succeeds, no call makes a transfer or a pin write;
 * - `InvalidArgument` means nothing was sent;
 * - nothing is ever written outside the requested range; only `Write()` changes the array and only
 *   `SetBlockProtection()` changes BP1:BP0;
 * - a call never returns `Ok` for bytes the chip does not hold: a `Read()` or `Write()` that
 *   returns `Ok` matches the chip unless at least two transfers of the call were tampered (one,
 *   with `Verify::None`);
 * - with `check_write_enable`, `EnsureInitialized()` never returns `Ok` on a dead bus (floating,
 *   stuck low, echo, HOLD) or with WP tied low, unless a transfer was tampered;
 * - WP is always parked after a call, unless the last pin write failed;
 * - a call that returns `Ok` leaves the write enable latch as clear as it found it, unless
 * tampered.
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang,
 * `-DAT25040B_BUILD_FUZZERS=ON`), and as a plain executable that replays the corpus in
 * tests/fuzz/corpus/ as a CTest on any compiler.
 */
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

#include "at25040b.hpp"
#include "mock_at25040b_bus.hpp"

namespace {

using at25040b::BlockProtect;
using at25040b::Error;
using at25040b_test::At25040bModel;
using at25040b_test::Miso;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }
  uint16_t Word() noexcept {
    const auto high = Byte();
    return static_cast<uint16_t>((static_cast<uint32_t>(high) << 8U) | Byte());
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a crash.
void Check(bool promise, int line = __builtin_LINE()) noexcept {
  if (!promise) {
    (void)std::fprintf(stderr, "fuzz_at25040b: promise broken at line %d\n", line);
    std::abort();
  }
}

/// The SPI port: the chip model behind a bus the fuzzer controls, and a WP pin.
class FuzzPort : public at25040b::SpiInterface<FuzzPort> {
public:
  explicit FuzzPort(Input& in) noexcept : in_(in) {
    chip_.wp_high = false; // the board's pull-down parks WP before anyone drives it
  }

  [[nodiscard]] bool EnsureInitialized() const noexcept {
    return bus_up_ok_;
  }

  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    Check(tx != nullptr && len >= 1 && len <= at25040b::kMaxTransferBytes);
    transfers_++;
    const uint8_t action = in_.Byte();
    if (action == 0xFFU) {
      return false; // the bus failed; the frame never reached the chip
    }
    const bool bus_not_chip = chip_.miso != Miso::Chip || chip_.hold_tied_to_cs;
    tampered_ += bus_not_chip ? 1 : 0;
    if (action == 0xFEU) {
      tampered_ += bus_not_chip ? 0 : 1; // lost: the chip never sees it, MISO floats
      const uint8_t level = (in_.Byte() & 1U) != 0U ? 0xFFU : 0x00U;
      for (std::size_t i = 0; rx != nullptr && i < len; ++i) {
        rx[i] = level;
      }
      return true;
    }
    chip_.Frame(tx, rx, len);
    if (rx != nullptr && (action == 0xFDU || action == 0xFCU)) {
      tampered_ += bus_not_chip ? 0 : 1;
      for (std::size_t i = 0; i < len; ++i) {
        rx[i] = action == 0xFDU ? static_cast<uint8_t>(rx[i] ^ in_.Byte()) : in_.Byte();
      }
    }
    return true; // any other value: the chip's own reply
  }

  void DelayUs(uint32_t us) noexcept {
    chip_.Elapse(us);
  }

  bool GpioSet(at25040b::CtrlPin pin, at25040b::GpioSignal signal) noexcept {
    Check(pin == at25040b::CtrlPin::WriteProtect);
    gpio_calls_++;
    last_gpio_failed_ = in_.Byte() == 0xFFU;
    if (!last_gpio_failed_) {
      chip_.wp_high = signal == at25040b::GpioSignal::INACTIVE;
    }
    return !last_gpio_failed_;
  }

  At25040bModel& Chip() noexcept {
    return chip_;
  }
  [[nodiscard]] int Transfers() const noexcept {
    return transfers_;
  }
  [[nodiscard]] int GpioCalls() const noexcept {
    return gpio_calls_;
  }
  [[nodiscard]] int Tampered() const noexcept {
    return tampered_;
  }
  [[nodiscard]] bool LastGpioFailed() const noexcept {
    return last_gpio_failed_;
  }
  void SetBusUp(bool ok) noexcept {
    bus_up_ok_ = ok;
  }

private:
  Input& in_;
  At25040bModel chip_;
  int transfers_ = 0;
  int gpio_calls_ = 0;
  int tampered_ = 0;
  bool last_gpio_failed_ = false;
  bool bus_up_ok_ = true;
};

using Driver = at25040b::AT25040B<FuzzPort>;

bool KnownError(Error e) noexcept {
  switch (e) {
  case Error::Ok:
  case Error::NotInitialized:
  case Error::InvalidArgument:
  case Error::Bus:
  case Error::Timeout:
  case Error::DeviceIdMismatch:
  case Error::NoDevice:
  case Error::BusNoise:
  case Error::WriteProtected:
  case Error::BlockProtected:
  case Error::WriteMismatch:
  case Error::Pin:
    return true;
  case Error::Crc: // documented as never returned
  case Error::Busy:
  case Error::DeviceFault:
  case Error::Unsupported:
    return false;
  }
  return false;
}

/// The `Verify` value the following `Read()` and `Write()` calls pass.
struct Verifies {
  at25040b::Verify read = at25040b::Verify::ReadBack;
  at25040b::Verify write = at25040b::Verify::ReadBack;
};

/// A configuration from the fuzzer's bytes, often invalid on purpose, and the `Verify` values of
/// the calls after it. They come from the bits that chose the configuration's verify options
/// before `Verify` replaced them, so the corpus keeps its meaning.
at25040b::Config FuzzConfig(Input& in, Verifies& v) noexcept {
  static constexpr std::array<uint32_t, 4> kTimeouts = {1000, 5000, 10000, 2000000};
  at25040b::Config c{};
  const uint8_t b = in.Byte();
  c.variant = static_cast<at25040b::Variant>(b % 4U); // 3 is not an enumerator
  c.check_write_enable = (b & 0x04U) == 0U;
  v.write = (b & 0x08U) == 0U ? at25040b::Verify::ReadBack : at25040b::Verify::None;
  v.read = (b & 0x10U) == 0U ? at25040b::Verify::ReadBack : at25040b::Verify::None;
  c.write_timeout_us = kTimeouts[(b >> 5U) % 4U];
  return c;
}

/// The board between calls: what a cable, a board or a worn part does to the chip.
void FuzzChip(FuzzPort& port, Input& in) noexcept {
  At25040bModel& chip = port.Chip();
  const uint8_t a = in.Byte();
  const uint8_t b = in.Byte();
  chip.miso = (a & 0x07U) < 5U ? Miso::Chip : static_cast<Miso>(1U + (a % 4U));
  chip.hold_tied_to_cs = (a & 0x18U) == 0x18U;
  chip.wp_tied_low = (a & 0x60U) == 0x60U;
  chip.ignore_writes = (b & 0x03U) == 0x03U ? 1 : 0;
  chip.stuck_address = (b & 0x0CU) == 0x0CU ? in.Byte() : -1;
  chip.write_cycle_us = (b & 0x30U) == 0x30U ? 20000U : 3000U;
  if ((b & 0xC0U) == 0x40U) {
    chip.busy_us = 2000U * (in.Byte() % 8U); // a cycle running with nothing to program
  } else if ((b & 0xC0U) == 0x80U) {
    chip.bp = static_cast<uint8_t>(in.Byte() % 4U);
  } else if ((b & 0xC0U) == 0xC0U) {
    chip.PowerCycle();
  }
  chip.noise_state = in.Word();
  port.SetBusUp((in.Byte() & 0x0FU) != 0U);
}

/// What a call may change, taken before it.
struct Before {
  std::array<uint8_t, 512> memory{};
  uint8_t bp = 0;
  bool wen = false;
  bool up = false;
  int transfers = 0;
  int gpio = 0;
  int tampered = 0;
};

Before Snapshot(const Driver& d, FuzzPort& port) noexcept {
  // A write cycle a timed-out call left running would program its bytes during a later call;
  // let it finish first, so that every change of the array belongs to the call that sent it.
  if (port.Chip().HasPendingWrite()) {
    port.Chip().Elapse(port.Chip().busy_us);
  }
  Before b{};
  b.memory = port.Chip().memory;
  b.bp = port.Chip().bp;
  b.wen = port.Chip().wen;
  b.up = d.IsInitialized();
  b.transfers = port.Transfers();
  b.gpio = port.GpioCalls();
  b.tampered = port.Tampered();
  return b;
}

/// Promises every call keeps.
void CheckCommon(const Driver& d, FuzzPort& port, const Before& b, Error e, bool lifecycle) {
  Check(KnownError(e));
  const int sent = port.Transfers() - b.transfers;
  const int pins = port.GpioCalls() - b.gpio;
  const int tampered = port.Tampered() - b.tampered;
  if (!b.up && !lifecycle) {
    Check(e == Error::NotInitialized && sent == 0 && pins == 0);
  }
  if (e == Error::InvalidArgument) {
    Check(sent == 0 && pins == 0);
  }
  Check(!port.Chip().wp_high || port.LastGpioFailed()); // WP parked
  if (e == Error::Ok && !b.wen && tampered == 0) {
    Check(!port.Chip().wen); // the latch left clear
  }
  (void)d;
}

void CheckUntouched(FuzzPort& port, const Before& b, std::size_t from, std::size_t to) {
  for (std::size_t i = 0; i < b.memory.size(); ++i) {
    if (i < from || i >= to) {
      Check(port.Chip().memory[i] == b.memory[i]); // nothing outside the range
    }
  }
}

bool DeadBus(const At25040bModel& chip) noexcept {
  return chip.miso == Miso::FloatHigh || chip.miso == Miso::StuckLow ||
         chip.miso == Miso::EchoMosi || chip.hold_tied_to_cs || chip.wp_tied_low;
}

void FuzzRead(Driver& d, FuzzPort& port, Input& in, const Before& b, at25040b::Verify verify) {
  const uint16_t address = in.Word() % 600U;
  const std::size_t length = in.Word() % 600U;
  std::array<uint8_t, 600> out{};
  out.fill(0xA5);
  const auto r = d.Read(address, std::span{out}.first(length), verify);
  CheckCommon(d, port, b, r.error, false);
  CheckUntouched(port, b, 0, 0);
  Check(port.Chip().bp == b.bp);
  const int tampered = port.Tampered() - b.tampered;
  const bool range_ok = r.error != Error::InvalidArgument && r.error != Error::NotInitialized;
  for (std::size_t i = 0; range_ok && i < length; ++i) {
    if (r.ok()) {
      const bool same = out[i] == port.Chip().memory[address + i];
      Check(same || tampered >= (verify == at25040b::Verify::ReadBack ? 2 : 1));
    } else {
      Check(out[i] == 0); // no partial data left behind
    }
  }
}

void FuzzWrite(Driver& d, FuzzPort& port, Input& in, const Before& b, at25040b::Verify verify) {
  const uint16_t address = in.Word() % 600U;
  const std::size_t length = in.Byte() % 40U;
  std::array<uint8_t, 40> data{};
  for (std::size_t i = 0; i < length; ++i) {
    data[i] = in.Byte();
  }
  const auto r = d.Write(address, std::span{data}.first(length), verify);
  CheckCommon(d, port, b, r.error, false);
  const std::size_t end = address + length;
  CheckUntouched(port, b, address, end);
  Check(port.Chip().bp == b.bp);
  if (r.ok()) {
    int tampered = port.Tampered() - b.tampered;
    const auto stuck = static_cast<std::size_t>(port.Chip().stuck_address);
    tampered += port.Chip().stuck_address >= 0 && stuck >= address && stuck < end ? 2 : 0;
    for (std::size_t i = 0; i < length; ++i) {
      const bool held = port.Chip().memory[address + i] == data[i];
      Check(held || tampered >= (verify == at25040b::Verify::ReadBack ? 2 : 1));
    }
  }
}

void RunOne(Driver& d, FuzzPort& port, Input& in, Verifies& v) {
  const uint8_t op_byte = in.Byte();
  const uint8_t op = op_byte % 10U;
  const Before b = Snapshot(d, port);
  const auto tampered = [&] { return port.Tampered() - b.tampered; };

  switch (op) {
  case 0: {
    const bool dead = DeadBus(port.Chip()) && d.GetConfig().check_write_enable;
    const auto r = d.EnsureInitialized();
    CheckCommon(d, port, b, r.error, true);
    Check(r.ok() == d.IsInitialized());
    if (b.up) {
      Check(r.ok() && port.Transfers() == b.transfers); // idempotent, no I/O once started
    } else if (r.ok() && dead) {
      Check(tampered() > 0); // a dead bus never passes the identity check by itself
    }
    CheckUntouched(port, b, 0, 0);
    Check(port.Chip().bp == b.bp);
    break;
  }
  case 1: {
    const auto r = d.Deinitialize();
    CheckCommon(d, port, b, r.error, true);
    Check(!d.IsInitialized());
    break;
  }
  case 2: {
    const auto r = d.ApplyConfig(FuzzConfig(in, v));
    CheckCommon(d, port, b, r.error, true);
    Check(port.Transfers() == b.transfers && port.GpioCalls() == b.gpio); // never any I/O
    break;
  }
  case 3:
    FuzzRead(d, port, in, b, v.read);
    break;
  case 4:
    FuzzWrite(d, port, in, b, v.write);
    break;
  case 5: {
    const auto level = static_cast<BlockProtect>(in.Byte() % 5U); // 4 is not a level
    const auto r = d.SetBlockProtection(level);
    CheckCommon(d, port, b, r.error, false);
    CheckUntouched(port, b, 0, 0);
    if (r.ok()) {
      Check(port.Chip().bp == static_cast<uint8_t>(level) || tampered() > 0);
    }
    break;
  }
  case 6: {
    const auto r = d.GetBlockProtection();
    CheckCommon(d, port, b, r.error, false);
    Check(r.ok() || r.value == BlockProtect::None);
    Check(!r.ok() || static_cast<uint8_t>(r.value) == port.Chip().bp || tampered() > 0);
    Check(port.Chip().bp == b.bp);
    break;
  }
  case 7: {
    // The tens digit of the op byte picks the decoded or the raw status read, so the corpus keeps
    // the meaning of every other operation.
    if ((op_byte / 10U) % 2U == 0U) {
      const auto r = d.ReadStatus();
      CheckCommon(d, port, b, r.error, false);
      Check(r.ok() || r.value.raw == 0);
    } else {
      const auto r = d.ReadRawStatus();
      CheckCommon(d, port, b, r.error, false);
      Check(r.ok() || r.value == 0);
      Check(!r.ok() || d.GetLastStatusByte() == r.value);
      Check(port.Transfers() - b.transfers <= 1); // one RDSR, never retried
    }
    Check(port.Chip().bp == b.bp);
    break;
  }
  default:
    FuzzChip(port, in);
    break;
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  FuzzPort port(in);
  Verifies verifies{};
  Driver driver(port, FuzzConfig(in, verifies));
  for (int calls = 0; calls < 64 && !in.Empty(); ++calls) {
    RunOne(driver, port, in, verifies);
  }
  return 0;
}
