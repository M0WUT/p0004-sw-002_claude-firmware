// The four Soft Keys under the LCD, numbered 1-4 left to right (issue #12).
// Active low. pytest can simulate presses by writing kOverrideAddr over SWD:
// (0x4B45 << 16) | mask, bit 0 = Soft Key 1. Until the CI rig presses the real
// keys (issue #34), this is how tests hold them.
#pragma once
#include <cstdint>

namespace gte {

class SoftKeys {
 public:
  static constexpr uint32_t kOverrideAddr =
      0x2001FFF4;  // DTCM, untouched by startup code

  void Begin();
  uint8_t Mask();  // bit n-1 set while Soft Key n is down
  bool IsDown(int key) { return Mask() & (1u << (key - 1)); }
  // Wait while `keys` (a mask) stay down; true once held for hold_ms, false if
  // released first.
  bool HeldFor(uint8_t keys, uint32_t hold_ms,
               void (*while_waiting)() = nullptr);
  void StartTask();  // logs presses and releases (INFO)

 private:
  static void Task(void *self);
};

}  // namespace gte
