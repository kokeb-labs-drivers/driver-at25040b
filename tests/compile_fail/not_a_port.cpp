/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: Port
 */
#include "at25040b.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  at25040b::AT25040B<NotAPort> eeprom(port);
  return static_cast<int>(eeprom.EnsureInitialized().error);
}
