#include <iostream>

#include "app/ModeOverrideWindow.h"
#include "app/ReplayGuard.h"
#include "app/RuleEngine.h"

namespace {

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::cerr << "CHECK failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
      return false; \
    } \
  } while (0)

bool test_boot_starts_disarm_without_entry_alarm() {
  RuleEngine engine;
  Config cfg;
  SystemState st;

  CHECK(st.mode == Mode::disarm);
  const Decision d = engine.handle(st, cfg, {EventType::door_open, 100, 1});

  CHECK(d.next.mode == Mode::disarm);
  CHECK(!d.next.entry_pending);
  CHECK(d.next.level == AlarmLevel::off);
  CHECK(d.cmd.type == CommandType::none);
  return true;
}

bool test_armed_door_open_starts_entry_countdown() {
  RuleEngine engine;
  Config cfg;
  SystemState st;
  st.mode = Mode::away;

  const uint32_t nowMs = 1000;
  const Decision d = engine.handle(st, cfg, {EventType::door_open, nowMs, 1});

  CHECK(d.next.mode == Mode::away);
  CHECK(d.next.entry_pending);
  CHECK(d.next.entry_deadline_ms == (nowMs + cfg.entry_delay_ms));
  CHECK(d.next.suspicion_score == 15);
  CHECK(d.next.level == AlarmLevel::warn);
  CHECK(d.cmd.type == CommandType::buzzer_warn);
  return true;
}

bool test_locked_door_open_escalates_alert_in_any_mode() {
  RuleEngine engine;
  Config cfg;

  SystemState disarmState;
  disarmState.mode = Mode::disarm;
  disarmState.door_locked = true;
  const Decision d1 = engine.handle(disarmState, cfg, {EventType::door_open, 1000, 1});
  CHECK(d1.next.level == AlarmLevel::alert);
  CHECK(d1.next.suspicion_score == 100);
  CHECK(!d1.next.entry_pending);
  CHECK(d1.cmd.type == CommandType::buzzer_alert);

  SystemState awayState;
  awayState.mode = Mode::away;
  awayState.door_locked = true;
  const Decision d2 = engine.handle(awayState, cfg, {EventType::door_open, 1100, 1});
  CHECK(d2.next.level == AlarmLevel::alert);
  CHECK(d2.next.suspicion_score == 100);
  CHECK(!d2.next.entry_pending);
  CHECK(d2.cmd.type == CommandType::buzzer_alert);
  return true;
}

bool test_mode_override_window_expires_and_handles_wraparound() {
  ModeOverrideWindow w;

  CHECK(!w.active(0));

  w.activate(1000, 50);
  CHECK(w.active(1049));
  CHECK(!w.active(1050));

  w.activate(0xFFFFFFF0u, 30);
  CHECK(w.active(5));
  CHECK(!w.active(20));

  w.activate(300, 0);
  CHECK(!w.active(301));
  return true;
}

bool test_replay_guard_blocks_replay_and_allows_after_expiry() {
  ReplayGuard g;

  CHECK(!g.accept("", 100, 30));
  CHECK(g.accept("nonce-a", 100, 30));
  CHECK(!g.accept("nonce-a", 110, 30));
  CHECK(g.accept("nonce-b", 110, 30));
  CHECK(g.accept("nonce-a", 131, 30));

  g = ReplayGuard{};
  CHECK(g.accept("wrap", 0xFFFFFFF0u, 30));
  CHECK(!g.accept("wrap", 5, 30));
  CHECK(g.accept("wrap", 31, 30));
  return true;
}

} // namespace

int main() {
  bool ok = true;

  ok &= test_boot_starts_disarm_without_entry_alarm();
  ok &= test_armed_door_open_starts_entry_countdown();
  ok &= test_locked_door_open_escalates_alert_in_any_mode();
  ok &= test_mode_override_window_expires_and_handles_wraparound();
  ok &= test_replay_guard_blocks_replay_and_allows_after_expiry();

  if (!ok) return 1;

  std::cout << "native_flow tests passed\n";
  return 0;
}
