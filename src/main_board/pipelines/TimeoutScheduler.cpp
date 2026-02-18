#include "pipelines/TimeoutScheduler.h"

bool TimeoutScheduler::pollEntryTimeout(const SystemState& st, uint32_t nowMs, Event& out) const {
  if (!st.entry_pending) return false;
  if ((int32_t)(nowMs - st.entry_deadline_ms) < 0) return false;
  out = {EventType::entry_timeout, nowMs, 0};
  return true;
}
