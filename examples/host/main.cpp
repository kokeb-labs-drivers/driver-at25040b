/**
 * @file main.cpp
 * @brief The AT25040B driver on your PC: check a simulated chip, write a record across a page
 * boundary, read it back, and see what an unplugged module looks like.
 *
 * No hardware and no SDK: the host port (host_at25040b_port.hpp) connects the real driver to the
 * protocol model the host tests use. The same calls run on an MCU with that MCU's port
 * (examples/esp32/ for ESP-IDF). Exit code 0 when every call did what it should.
 */
#include <array>
#include <cstdint>
#include <cstdio>

#include "at25040b.hpp"
#include "host_at25040b_port.hpp"

int main() {
  HostAt25040bPort port;
  at25040b::AT25040B<HostAt25040bPort> eeprom(port); // no I/O yet

  std::printf("AT25040B driver %s on the host\n", at25040b::GetDriverVersion());
  if (const auto r = eeprom.EnsureInitialized(); !r) {
    std::printf("EnsureInitialized failed: %s\n", at25040b::ToString(r.error));
    return 1;
  }

  // 12 bytes from 0x0FC: the record crosses a page boundary and the A8 boundary at 0x100. It has
  // no CRC, so each page is read back (Verify::ReadBack); the default sends no read-back.
  const std::array<uint8_t, 12> record = {'k', 'o', 'k', 'e', 'b', '-', 'a', 't', '2', '5', 0, 1};
  if (const auto r = eeprom.Write(0x0FC, record, at25040b::Verify::ReadBack); !r) {
    std::printf("Write failed: %s\n", at25040b::ToString(r.error));
    return 1;
  }
  std::printf("wrote %u bytes at 0x0FC in %d write cycles\n", static_cast<unsigned>(record.size()),
              port.state.chip.write_cycles);

  std::array<uint8_t, 12> back{};
  if (const auto r = eeprom.Read(0x0FC, back); !r || back != record) {
    std::printf("Read back failed\n");
    return 1;
  }
  std::printf("read back:");
  for (const uint8_t b : back) {
    std::printf(" %02X", static_cast<unsigned>(b));
  }
  std::printf("\n");

  // Unplug the module: MISO floats high. A handler probes again on every attach.
  port.state.chip.miso = at25040b_test::Miso::FloatHigh;
  (void)eeprom.Deinitialize();
  const auto probe = eeprom.EnsureInitialized();
  std::printf("after unplugging: %s, status byte 0x%02X\n", at25040b::ToString(probe.error),
              static_cast<unsigned>(eeprom.GetLastStatusByte()));
  return probe.error == at25040b::Error::NoDevice ? 0 : 1;
}
