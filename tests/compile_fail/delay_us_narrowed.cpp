/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would truncate long waits, so
 * the driver rejects it instead of using it (handbook KB-CPP20-007).
 *
 * EXPECT: DelayUs hook must be
 */
#include <cstdint>

#include "at25040b.hpp"
#include "mock_at25040b_bus.hpp"

class ShortDelayPort : public at25040b_test::MockPortBase<ShortDelayPort> {
public:
  void DelayUs(uint16_t us) noexcept {
    (void)us;
  }
};

int main() {
  ShortDelayPort port;
  at25040b::AT25040B<ShortDelayPort> eeprom(port);
  return static_cast<int>(eeprom.EnsureInitialized().error);
}
