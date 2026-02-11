#include "Logger.h"

void Logger::begin() {}

void Logger::update(uint32_t) {}

void Logger::logCommand(const Command& cmd, const SystemState& st) {
  Serial.print("[CMD] "); Serial.print((int)cmd.type);
  Serial.print(" | MODE "); Serial.print((int)st.mode);
  Serial.print(" | LEVEL "); Serial.println((int)st.level);
}
