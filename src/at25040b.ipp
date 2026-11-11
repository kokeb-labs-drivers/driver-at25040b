/**
 * @file at25040b.ipp
 * @brief AT25040B driver implementation, included by `at25040b.hpp`.
 * @copyright Copyright (c) 2026 The driver-at25040b authors. SPDX-License-Identifier: MIT
 *
 * Section and table numbers refer to the AT25010B/020B/040B datasheet, Atmel-8707F (01/2015).
 */
#ifndef AT25040B_IPP
#define AT25040B_IPP

namespace at25040b {

// -------------------------------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> AT25040B<Port>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  if (auto r = initialize(); !r) {
    return r;
  }
  initialized_ = true;
  detail::Log(bus_.GetPort(), LogLevel::Info, "at25040b: initialised");
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> AT25040B<Port>::initialize() noexcept {
  if (Validate(config_) != Error::Ok) {
    return fail(Error::InvalidArgument, "at25040b: invalid Config");
  }
  if (!bus_.GetPort().EnsureInitialized()) {
    return fail(Error::Bus, "at25040b: the port could not bring the bus up");
  }
  if (auto r = parkWriteProtect(); !r) {
    return fail(r.error, "at25040b: WP could not be parked");
  }
  // (a) The status, read until a write cycle left over from before an MCU reset has ended, then
  // confirmed by a second read: both the same idle status (Table 6-3). 0xFF throughout is a
  // floating MISO, 0x05 the RDSR opcode echoed from MOSI; neither is an idle status.
  const auto first = waitIdle(false);
  if (!first) {
    return fail(first.error, (first.error == Error::NoDevice)
                                 ? "at25040b: status reads 0xFF: nothing drives MISO"
                                 : "at25040b: status is not a steady AT25xxxB status");
  }
  // (b) The write enable latch must follow WREN and WRDI. A MISO stuck low never shows WEN = 1.
  if (config_.check_write_enable) {
    const uint8_t idle = first.value;
    if (auto r = withWriteAccess([this, idle] { return checkWriteEnableToggle(idle); }); !r) {
      return fail(r.error,
                  "at25040b: write enable check failed (WEN did not follow WREN and WRDI)");
    }
    return Result<void>::Success();
  }
  // Without (b), for a board whose WP is tied low: the first bytes must read the same twice.
  if (auto r = checkStableHead(); !r) {
    return fail(r.error, "at25040b: the first bytes read differently twice");
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> AT25040B<Port>::checkWriteEnableToggle(uint8_t idle) noexcept {
  // WREN needs WP high; WRDI does not depend on WP (section 6, "Write Enable", "Write Disable").
  const auto base =
      static_cast<uint8_t>(static_cast<uint32_t>(idle) & ~static_cast<uint32_t>(reg::kStatusWen));
  if ((static_cast<uint32_t>(idle) & static_cast<uint32_t>(reg::kStatusWen)) != 0U) {
    // The latch is already set (a WREN before an MCU reset): clear it first, or WEN = 1 after our
    // WREN would prove nothing about WP. Found by the fuzzer.
    if (auto r = expectAfter(reg::kWrdi, base); !r) {
      return r;
    }
  }
  const auto with_wen =
      static_cast<uint8_t>(static_cast<uint32_t>(base) | static_cast<uint32_t>(reg::kStatusWen));
  if (auto r = expectAfter(reg::kWren, with_wen); !r) {
    disableWrite();
    return r;
  }
  return expectAfter(reg::kWrdi, base);
}

template <SpiPort Port>
Result<void> AT25040B<Port>::expectAfter(uint8_t opcode, uint8_t expected) noexcept {
  if (const Error e = bus_.Instruction(opcode); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  const auto status = readStatusByte();
  if (!status) {
    return Result<void>::Failure(status.error);
  }
  if (status.value != expected) {
    return Result<void>::Failure(detail::ToggleError(status.value, expected));
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> AT25040B<Port>::checkStableHead() noexcept {
  std::array<uint8_t, kIdentityReadBytes> first{};
  std::array<uint8_t, kIdentityReadBytes> again{};
  if (const Error e = bus_.Read(0U, std::span{first}); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  if (const Error e = bus_.Read(0U, std::span{again}); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  return first == again ? Result<void>::Success() : Result<void>::Failure(Error::BusNoise);
}

template <SpiPort Port>
Result<void> AT25040B<Port>::Deinitialize() noexcept {
  if (!initialized_) {
    return Result<void>::Success();
  }
  initialized_ = false;
  // Both steps run whatever the first returned. A failed park wins (KB-CPP-047): WP left released
  // is the state the caller must act on, and it is logged.
  const Error wrdi = bus_.Instruction(reg::kWrdi);
  if (auto parked = parkWriteProtect(); !parked) {
    return fail(parked.error, "at25040b: WP could not be parked");
  }
  if (wrdi != Error::Ok) {
    return Result<void>::Failure(wrdi);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> AT25040B<Port>::ApplyConfig(const Config& config) noexcept {
  if (Validate(config) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  config_ = config;
  return Result<void>::Success();
}

// -------------------------------------------------------------------------------------------------
// Memory
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> AT25040B<Port>::Read(uint16_t address, std::span<uint8_t> out,
                                  Verify verify) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!inRange(address, out.size()) || !detail::IsEnumerator(verify)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  auto r = readRange(address, out, verify);
  if (!r) {
    // std::fill returns nothing; std::ranges::fill returns an iterator that would be discarded
    // (MISRA C++:2023 Rule 0.1.2).
    std::fill(out.begin(), out.end(), uint8_t{0});
    return fail(r.error, "at25040b: read failed");
  }
  return r;
}

template <SpiPort Port>
Result<void> AT25040B<Port>::readRange(uint16_t address, std::span<uint8_t> out,
                                       Verify verify) noexcept {
  // The chip ignores everything but RDSR during a write cycle (section 6, "Write Sequence").
  if (auto s = waitIdle(true); !s) {
    return Result<void>::Failure(s.error);
  }
  const std::size_t total = out.size();
  std::size_t done = 0U;
  while (done < total) {
    const std::size_t left = total - done;
    const std::size_t n = (left < kReadChunkBytes) ? left : kReadChunkBytes;
    const auto at = static_cast<uint16_t>(static_cast<std::size_t>(address) + done);
    const std::span<uint8_t> chunk = out.subspan(done, n);
    if (const Error e = bus_.Read(at, chunk); e != Error::Ok) {
      return Result<void>::Failure(e);
    }
    if (verify == Verify::ReadBack) {
      std::array<uint8_t, kReadChunkBytes> again{};
      const std::span<uint8_t> second = std::span{again}.first(n);
      if (const Error e = bus_.Read(at, second); e != Error::Ok) {
        return Result<void>::Failure(e);
      }
      for (std::size_t i = 0U; i < n; ++i) {
        if (second[i] != chunk[i]) {
          return Result<void>::Failure(Error::BusNoise);
        }
      }
    }
    done += n;
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> AT25040B<Port>::Write(uint16_t address, std::span<const uint8_t> data,
                                   Verify verify) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!inRange(address, data.size()) || !detail::IsEnumerator(verify)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = writeRange(address, data, verify); !r) {
    return fail(r.error, "at25040b: write failed");
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> AT25040B<Port>::writeRange(uint16_t address, std::span<const uint8_t> data,
                                        Verify verify) noexcept {
  const auto status = waitIdle(true);
  if (!status) {
    return Result<void>::Failure(status.error);
  }
  // The chip would ignore a write into the protected part (section 6, "Write Sequence", Table 6-4):
  // refuse it before WREN, so nothing but the status read was sent.
  const BlockProtect level = detail::DecodeBlockProtect(status.value);
  if ((static_cast<std::size_t>(address) + data.size()) > ProtectedFrom(config_.variant, level)) {
    return Result<void>::Failure(Error::BlockProtected);
  }
  return withWriteAccess([this, address, data, verify] {
    const std::size_t total = data.size();
    std::size_t done = 0U;
    while (done < total) {
      // Only the three low address bits count up inside a WRITE (section 6, "Write Sequence"):
      // a write that crossed a page boundary would wrap to the start of the page.
      const auto at = static_cast<uint16_t>(static_cast<std::size_t>(address) + done);
      const std::size_t room = kPageSizeBytes - (static_cast<std::size_t>(at) % kPageSizeBytes);
      const std::size_t left = total - done;
      const std::size_t n = (left < room) ? left : room;
      if (auto r = writePage(at, data.subspan(done, n), verify); !r) {
        return r;
      }
      done += n;
    }
    return Result<void>::Success();
  });
}

template <SpiPort Port>
Result<void> AT25040B<Port>::writePage(uint16_t address, std::span<const uint8_t> data,
                                       Verify verify) noexcept {
  // WREN and its WEN check before every page stay on the hot path: the latch clears at the end of
  // each write cycle, so the datasheet requires the WREN, and the check costs one RDSR.
  if (auto r = enableWrite(); !r) {
    return r;
  }
  if (const Error e = bus_.Write(address, data); e != Error::Ok) {
    disableWrite();
    return Result<void>::Failure(e);
  }
  const auto after = finishWriteCycle();
  if (!after) {
    return Result<void>::Failure(after.error);
  }
  if (verify == Verify::ReadBack) {
    std::array<uint8_t, kPageSizeBytes> back{};
    const std::span<uint8_t> read_back = std::span{back}.first(data.size());
    if (const Error e = bus_.Read(address, read_back); e != Error::Ok) {
      return Result<void>::Failure(e);
    }
    const std::size_t count = data.size();
    for (std::size_t i = 0U; i < count; ++i) {
      if (read_back[i] != data[i]) {
        return Result<void>::Failure(Error::WriteMismatch);
      }
    }
  }
  return Result<void>::Success();
}

// -------------------------------------------------------------------------------------------------
// Status and block protection
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<Status> AT25040B<Port>::ReadStatus() noexcept {
  if (!initialized_) {
    return Result<Status>::Failure(Error::NotInitialized);
  }
  const auto raw = readStatusByte();
  if (!raw) {
    return Result<Status>::Failure(raw.error);
  }
  Status s{};
  s.raw = raw.value;
  if (raw.value == reg::kStatusBusyByte) {
    s.busy = true;
    return Result<Status>::Success(s);
  }
  if (!reg::IsIdleStatus(raw.value)) {
    return Result<Status>::Failure(Error::BusNoise);
  }
  s.write_enabled =
      (static_cast<uint32_t>(raw.value) & static_cast<uint32_t>(reg::kStatusWen)) != 0U;
  s.block_protect = detail::DecodeBlockProtect(raw.value);
  return Result<Status>::Success(s);
}

template <SpiPort Port>
Result<uint8_t> AT25040B<Port>::ReadRawStatus() noexcept {
  if (!initialized_) {
    return Result<uint8_t>::Failure(Error::NotInitialized);
  }
  return readStatusByte();
}

template <SpiPort Port>
Result<BlockProtect> AT25040B<Port>::GetBlockProtection() noexcept {
  if (!initialized_) {
    return Result<BlockProtect>::Failure(Error::NotInitialized);
  }
  const auto status = waitIdle(true);
  if (!status) {
    return Result<BlockProtect>::Failure(status.error);
  }
  return Result<BlockProtect>::Success(detail::DecodeBlockProtect(status.value));
}

template <SpiPort Port>
Result<void> AT25040B<Port>::SetBlockProtection(BlockProtect level) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!detail::IsEnumerator(level)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const auto status = waitIdle(true);
  if (!status) {
    return fail(status.error, "at25040b: status read failed");
  }
  if (detail::DecodeBlockProtect(status.value) == level) {
    return Result<void>::Success(); // already set: no write cycle spent on it
  }
  if (auto r = withWriteAccess([this, level] { return writeStatusRegister(level); }); !r) {
    return fail(r.error, "at25040b: block protection change failed");
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> AT25040B<Port>::writeStatusRegister(BlockProtect level) noexcept {
  // BP1 and BP0 are non-volatile cells: WREN first, then a write cycle (section 6, "Write Status
  // Register"). Bit 7 is not writable (revision 8707D); the driver writes only BP1 and BP0.
  if (auto r = enableWrite(); !r) {
    return r;
  }
  if (const Error e = bus_.WriteStatus(detail::EncodeBlockProtect(level)); e != Error::Ok) {
    disableWrite();
    return Result<void>::Failure(e);
  }
  const auto after = waitIdle(true);
  if (!after) {
    disableWrite();
    return Result<void>::Failure(after.error);
  }
  if ((static_cast<uint32_t>(after.value) & static_cast<uint32_t>(reg::kStatusWen)) != 0U) {
    disableWrite(); // the datasheet does not say that WRSR clears WEN; never leave it set
  }
  if (detail::DecodeBlockProtect(after.value) != level) {
    return Result<void>::Failure(Error::WriteMismatch);
  }
  return Result<void>::Success();
}

// -------------------------------------------------------------------------------------------------
// Private
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<uint8_t> AT25040B<Port>::readStatusByte() noexcept {
  uint8_t status = reg::kStatusBusyByte;
  if (const Error e = bus_.ReadStatus(status); e != Error::Ok) {
    return Result<uint8_t>::Failure(e);
  }
  last_status_ = status;
  return Result<uint8_t>::Success(status);
}

template <SpiPort Port>
Result<uint8_t> AT25040B<Port>::waitIdle(bool started) noexcept {
  // Every bit reads 1 during a write cycle, bits 7 to 4 read 0 otherwise (Table 6-3). Poll until
  // the cycle ends, at most write_timeout_us (t_WC is 5 ms at most, Table 4-3). An idle status
  // counts only when the next read says the same: one corrupted read must not let the driver send
  // an instruction the chip would ignore (a READ during the cycle reads high impedance). Found by
  // the fuzzer.
  uint32_t waited_us = 0U;
  for (;;) {
    const auto status = readStatusByte();
    if (!status) {
      return status;
    }
    if (status.value != reg::kStatusBusyByte) {
      if (!reg::IsIdleStatus(status.value)) {
        return Result<uint8_t>::Failure(started ? Error::BusNoise : Error::DeviceIdMismatch);
      }
      const auto again = readStatusByte();
      if (!again || (again.value == status.value)) {
        return again;
      }
      if (again.value != reg::kStatusBusyByte) {
        return Result<uint8_t>::Failure(Error::BusNoise);
      }
    }
    if (waited_us >= config_.write_timeout_us) {
      return Result<uint8_t>::Failure(started ? Error::Timeout : Error::NoDevice);
    }
    detail::DelayUs(bus_.GetPort(), kPollIntervalUs);
    waited_us += kPollIntervalUs;
  }
}

template <SpiPort Port>
Result<void> AT25040B<Port>::enableWrite() noexcept {
  // Every WRITE and WRSR needs WREN first; the latch clears at the end of each write cycle
  // (section 6, "Write Enable", "Write Sequence"). WREN with WP low leaves WEN at 0.
  if (const Error e = bus_.Instruction(reg::kWren); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  const auto status = readStatusByte();
  if (!status) {
    disableWrite();
    return Result<void>::Failure(status.error);
  }
  if (!reg::IsIdleStatus(status.value)) {
    disableWrite();
    return Result<void>::Failure(Error::BusNoise);
  }
  if ((static_cast<uint32_t>(status.value) & static_cast<uint32_t>(reg::kStatusWen)) == 0U) {
    return Result<void>::Failure(Error::WriteProtected);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<uint8_t> AT25040B<Port>::finishWriteCycle() noexcept {
  const auto status = waitIdle(true);
  if (!status) {
    disableWrite();
    return status;
  }
  // A write cycle always ends with WEN = 0 (section 6, "Write Sequence"). Still set: the chip
  // ignored the WRITE (WP dropped, or the frame never reached it), and no cycle ran.
  if ((static_cast<uint32_t>(status.value) & static_cast<uint32_t>(reg::kStatusWen)) != 0U) {
    disableWrite();
    return Result<uint8_t>::Failure(Error::WriteMismatch);
  }
  return status;
}

template <SpiPort Port>
void AT25040B<Port>::disableWrite() noexcept {
  // Best effort after a failure: the call already fails with the first error.
  (void)bus_.Instruction(reg::kWrdi);
}

template <SpiPort Port>
Result<void> AT25040B<Port>::releaseWriteProtect() noexcept {
  if constexpr (detail::HasGpioSet<Port>) {
    if (!bus_.GetPort().GpioSet(CtrlPin::WriteProtect, GpioSignal::INACTIVE)) {
      return Result<void>::Failure(Error::Pin);
    }
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> AT25040B<Port>::parkWriteProtect() noexcept {
  if constexpr (detail::HasGpioSet<Port>) {
    if (!bus_.GetPort().GpioSet(CtrlPin::WriteProtect, GpioSignal::ACTIVE)) {
      return Result<void>::Failure(Error::Pin);
    }
  }
  return Result<void>::Success();
}

template <SpiPort Port>
template <detail::WriteAccessOp Op>
Result<void> AT25040B<Port>::withWriteAccess(Op op) noexcept {
  // WP high only for the instructions that need it (section 5, "Write Protect"), parked after
  // whatever happened.
  Result<void> result = releaseWriteProtect();
  if (result) {
    result = op();
  }
  // A failed park is reported even when the step failed first: WP may be left released, which
  // matters more than why the step failed (KB-CPP-047). The step's failure is logged by the caller.
  if (auto parked = parkWriteProtect(); !parked) {
    return fail(parked.error, "at25040b: WP could not be parked");
  }
  return result;
}

template <SpiPort Port>
bool AT25040B<Port>::inRange(uint16_t address, std::size_t length) const noexcept {
  const std::size_t size = CapacityBytes(config_.variant);
  return (length > 0U) && (address < size) && (length <= (size - address));
}

template <SpiPort Port>
Result<void> AT25040B<Port>::fail(Error error, const char* message) noexcept {
  detail::Log(bus_.GetPort(), LogLevel::Error, message);
  return Result<void>::Failure(error);
}

} // namespace at25040b

#endif // AT25040B_IPP
