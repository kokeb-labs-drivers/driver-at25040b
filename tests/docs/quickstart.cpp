/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include <array>
#include <cstdint>

#include "at25040b.hpp"

int read_board_id(MyPort& port, std::array<uint8_t, 8>& id) {
  at25040b::AT25040B<MyPort> eeprom(port); // AT25040B, identity check on; no I/O yet

  if (auto r = eeprom.EnsureInitialized(); !r) { // 1. bus up, identity check
    return static_cast<int>(r.error); //    NoDevice: nothing on MISO. DeviceIdMismatch: wiring, WP
  }
  if (auto r = eeprom.Read(0, id); !r) { // 2. eight bytes from address 0
    return static_cast<int>(r.error);    //    Timeout: a write cycle that never ends
  }
  return 0; // 3. id holds what the chip sent: one READ, so check its CRC
}
// --8<-- [end:quickstart]
