#include "Logger.h"

void Logger::begin() {}

void Logger::update(uint32_t) {}

void Logger::logCommand(const Command& cmd, const SystemState& st) {
  if (cmd.type == CommandType::none) return;
  Serial.print("[LOG] command.type="); Serial.println(toString(cmd.type));
  Serial.print("[LOG] state.mode="); Serial.println(toString(st.mode));
  Serial.print("[LOG] state.level="); Serial.println(toString(st.level));
}
