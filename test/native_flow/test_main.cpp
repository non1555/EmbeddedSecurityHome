#include <iostream>

#include "app/ModeOverrideWindow.h"
#include "app/ReplayGuard.h"
#include "app/RuleEngine.h"
#include "auto_board/automation/presence.h"

namespace {

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::cerr << "CHECK failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
      return false; \
    } \
  } while (0)

bool test_boot_starts_startup_safe_without_entry_alarm() {
  RuleEngine engine;
  Config cfg;
  SystemState st;

  CHECK(st.mode == Mode::startup_safe);
  const Decision d = engine.handle(st, cfg, {EventType::door_open, 100, 1});

  CHECK(d.next.mode == Mode::startup_safe);
  CHECK(!d.next.entry_pending);
  CHECK(d.next.level == AlarmLevel::off);
  CHECK(d.cmd.type == CommandType::none);
  return true;
}

bool test_armed_door_open_starts_entry_countdown() {
  RuleEngine engine;
  Config cfg;
  SystemState st;
  st.mode = Mode::night;

  const uint32_t nowMs = 1000;
  const Decision d = engine.handle(st, cfg, {EventType::door_open, nowMs, 1});

  CHECK(d.next.mode == Mode::night);
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

  SystemState nightState;
  nightState.mode = Mode::night;
  nightState.door_locked = true;
  const Decision d2 = engine.handle(nightState, cfg, {EventType::door_open, 1100, 1});
  CHECK(d2.next.level == AlarmLevel::alert);
  CHECK(d2.next.suspicion_score == 100);
  CHECK(!d2.next.entry_pending);
  CHECK(d2.cmd.type == CommandType::buzzer_alert);
  return true;
}

bool test_presence_entry_unlock_ultrasonic_pir_marks_home() {
  Presence::Config cfg;
  cfg.unlock_to_ultrasonic_ms = 100;
  cfg.entry_pir_ms = 120;
  cfg.exit_sequence_ms = 100;
  cfg.away_no_pir_ms = 50;
  cfg.away_revert_pir_ms = 40;

  Presence::init(cfg);
  CHECK(Presence::state() == Presence::State::unknown);

  Presence::onDoorUnlock(10);
  Presence::onDoorUltrasonic(40);
  Presence::onPirDetected(100);

  CHECK(Presence::state() == Presence::State::home);
  CHECK(Presence::isHome());
  CHECK(isSomeoneHome);
  return true;
}

bool test_presence_exit_sequence_marks_away_after_no_pir() {
  Presence::Config cfg;
  cfg.unlock_to_ultrasonic_ms = 100;
  cfg.entry_pir_ms = 120;
  cfg.exit_sequence_ms = 80;
  cfg.away_no_pir_ms = 50;
  cfg.away_revert_pir_ms = 40;

  Presence::init(cfg);
  CHECK(Presence::state() == Presence::State::unknown);

  Presence::onDoorUltrasonic(100);
  Presence::onDoorOpen(120);
  Presence::onDoorClose(140);

  Presence::tick(191);
  CHECK(Presence::state() == Presence::State::away);
  CHECK(!Presence::isHome());
  CHECK(!isSomeoneHome);
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

  ok &= test_boot_starts_startup_safe_without_entry_alarm();
  ok &= test_armed_door_open_starts_entry_countdown();
  ok &= test_locked_door_open_escalates_alert_in_any_mode();
  ok &= test_presence_entry_unlock_ultrasonic_pir_marks_home();
  ok &= test_presence_exit_sequence_marks_away_after_no_pir();
  ok &= test_mode_override_window_expires_and_handles_wraparound();
  ok &= test_replay_guard_blocks_replay_and_allows_after_expiry();

  if (!ok) return 1;

  std::cout << "native_flow tests passed\n";
  return 0;
}
