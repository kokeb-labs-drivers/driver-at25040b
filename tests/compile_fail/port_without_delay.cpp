/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: the status polls and write cycles need the port's DelayUs().
 *
 * EXPECT: DelayUs
 */
#include "at25040b.hpp"
#include "mock_at25040b_bus.hpp"

class NoDelayPort : public at25040b_test::MockPortBase<NoDelayPort> {};

int main() {
  NoDelayPort port;
  at25040b::AT25040B<NoDelayPort> eeprom(port);
  return static_cast<int>(eeprom.EnsureInitialized().error);
}
