/**
 * @file gpio_set_returns_void.cpp
 * @brief Must NOT compile: an optional hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: GpioSet hook must be
 */
#include "at25040b.hpp"
#include "mock_at25040b_bus.hpp"

class VoidGpioPort : public at25040b_test::MockPortBase<VoidGpioPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
  void GpioSet(at25040b::CtrlPin pin, at25040b::GpioSignal signal) noexcept {
    (void)pin;
    (void)signal;
  }
};

int main() {
  VoidGpioPort port;
  at25040b::AT25040B<VoidGpioPort> eeprom(port);
  return static_cast<int>(eeprom.EnsureInitialized().error);
}
