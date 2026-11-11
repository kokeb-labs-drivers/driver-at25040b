/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version header and .ipp.
 */
#include <cstddef>
#include <cstdint>

#include "at25040b.hpp"

namespace {

// A port that answers every read with 0: enough to instantiate the whole driver template.
class NullPort : public at25040b::SpiInterface<NullPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    (void)tx;
    for (std::size_t i = 0; rx != nullptr && i < len; ++i) {
      rx[i] = 0;
    }
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

} // namespace

int main() {
  NullPort port;
  at25040b::AT25040B<NullPort> eeprom(port);
  // MISO reads 0 (a dead bus), so WEN never follows WREN and EnsureInitialized() must fail with
  // DeviceIdMismatch: proof the template compiled and ran.
  const auto r = eeprom.EnsureInitialized();
  return (r.error == at25040b::Error::DeviceIdMismatch && at25040b::GetDriverVersion()[0] != '\0')
             ? 0
             : 1;
}
