#pragma once
#include <stdint.h>

// Shared flag consumed by other modules (e.g., light control).
extern bool isSomeoneHome;

namespace Presence {

enum class State : uint8_t {
  unknown = 0,
  home = 1,
  away = 2,
};

struct Config {
  // unlock -> ultrasonic must happen within this window.
  uint32_t unlock_to_ultrasonic_ms = 60000;
  // ultrasonic -> PIR for confirming entry/home.
  uint32_t entry_pir_ms = 45000;

  // ultrasonic -> door open -> door close sequence window.
  uint32_t exit_sequence_ms = 45000;
  // after door close, no PIR for this long => away.
  uint32_t away_no_pir_ms = 120000;
  // if PIR appears shortly after away, revert to home.
  uint32_t away_revert_pir_ms = 30000;
};

void init(const Config& cfg = Config{});

void onDoorUnlock(uint32_t nowMs);
void onDoorUltrasonic(uint32_t nowMs);
void onDoorOpen(uint32_t nowMs);
void onDoorClose(uint32_t nowMs);
void onPirDetected(uint32_t nowMs);

void tick(uint32_t nowMs);
void setExternalHome(bool home, uint32_t nowMs);

State state();
bool isHome();

} // namespace Presence
