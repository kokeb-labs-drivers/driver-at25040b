/**
 * @file api_tasks.cpp
 * @brief The API guide's examples, compiled by the host build (never run).
 */
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

#include "at25040b.hpp"
#include "my_port.hpp"

using Eeprom = at25040b::AT25040B<MyPort>;

// --8<-- [start:lifecycle]
at25040b::Error BringUp(Eeprom& eeprom) {
  // The one entry point: checks the chip on the first call, then returns Ok with no I/O. After a
  // failure the next call starts again from the beginning.
  const auto r = eeprom.EnsureInitialized();
  if (!r) {
    std::printf("AT25040B: %s, status byte 0x%02X\n", at25040b::ToString(r.error),
                static_cast<unsigned>(eeprom.GetLastStatusByte()));
  }
  return r.error;
}
// --8<-- [end:lifecycle]

// --8<-- [start:read-image]
at25040b::Error ReadImage(Eeprom& eeprom, std::array<uint8_t, 512>& image) {
  // Any length inside the array: the driver waits first if a write cycle is still running, then
  // splits it into 16-byte READs. Verify::ReadBack reads each twice and compares (BusNoise): use it
  // when the image carries no CRC of its own; the default reads each once.
  return eeprom.Read(0, image, at25040b::Verify::ReadBack).error;
}
// --8<-- [end:read-image]

// --8<-- [start:write]
at25040b::Error WriteRecord(Eeprom& eeprom, uint16_t address, std::span<const uint8_t> data) {
  // Split at 8-byte pages, WREN per page, wait for each write cycle; Verify::ReadBack reads each
  // page back (WriteMismatch). With a GpioSet() hook the driver raises WP for the call and parks
  // it low after.
  const auto r = eeprom.Write(address, data, at25040b::Verify::ReadBack);
  if (r.error == at25040b::Error::BlockProtected) {
    std::printf("the range is block-protected: nothing was sent\n");
  }
  return r.error;
}
// --8<-- [end:write]

// --8<-- [start:protect]
at25040b::Error LockUpperHalf(Eeprom& eeprom) {
  // Non-volatile: one write cycle, then the level is read back. Nothing happens if it is set.
  if (auto r = eeprom.SetBlockProtection(at25040b::BlockProtect::UpperHalf); !r) {
    return r.error; // WriteProtected: WP low. WriteMismatch: the level did not take
  }
  const auto level = eeprom.GetBlockProtection();
  return level ? at25040b::Error::Ok : level.error;
}
// --8<-- [end:protect]

// --8<-- [start:errors]
const char* Describe(at25040b::Error e) {
  switch (e) {
  case at25040b::Error::NoDevice:
    return "nothing drives MISO: module unplugged, CS or VCC missing";
  case at25040b::Error::DeviceIdMismatch:
    return "the bus answers, but not like an AT25xxxB: MISO stuck or echoing, WP held low";
  case at25040b::Error::BusNoise:
    return "two reads disagreed: contention or noise on MISO, lower the clock";
  case at25040b::Error::WriteProtected:
    return "WP is low: release it, or the board ties it low";
  case at25040b::Error::WriteMismatch:
    return "the chip did not keep the write: retry once, then replace the part";
  default:
    return at25040b::ToString(e);
  }
}
// --8<-- [end:errors]

// --8<-- [start:reprobe]
at25040b::Error Reprobe(Eeprom& eeprom) {
  // A handler probes again on every attach: Deinitialize() (WRDI, WP parked) makes the next
  // EnsureInitialized() run the identity check again.
  (void)eeprom.Deinitialize();
  return eeprom.EnsureInitialized().error;
}
// --8<-- [end:reprobe]
