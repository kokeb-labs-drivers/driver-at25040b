/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "at25040b.hpp"
#include "at25040b_kconfig.hpp"
#include "my_port.hpp"

// --8<-- [start:custom]
at25040b::Config ReadOnlyBoard() {
  at25040b::Config c{};
  c.variant = at25040b::Variant::At25020b; // 256 bytes: every address is checked against it
  c.check_write_enable = false;            // WP tied low on this board: the toggle cannot pass
  return c;                                // at25040b::Validate(c) checks it without the chip
}

at25040b::Error StartReadOnly(MyPort& port) {
  static at25040b::AT25040B<MyPort> eeprom(port, ReadOnlyBoard()); // stored, no I/O
  return eeprom.EnsureInitialized().error;
}
// --8<-- [end:custom]

// --8<-- [start:kconfig]
at25040b::Error StartFromMenuconfig(MyPort& port) {
  static at25040b::AT25040B<MyPort> eeprom(port, at25040b::KconfigDefaults());
  return eeprom.EnsureInitialized().error;
}
// --8<-- [end:kconfig]
