#pragma once

#include "configuration_shared_types/Types.h"

namespace ApplicationLogicLayer {

class SystemContext;

class RuleEngine {
public:
  void process(const ConfigurationSharedTypes::Event& e, SystemContext& context) const; // ประมวลผลอีเวนต์ 1 รายการแบบครบวงจรและอัปเดตผ่าน context
  ConfigurationSharedTypes::Decision handle(const ConfigurationSharedTypes::SystemState& st,
                                            const ConfigurationSharedTypes::Event& e) const; // ประเมินกฎ state machine แล้วคืน decision ของอีเวนต์นั้น

  bool processDisarmAutoArmTick(ConfigurationSharedTypes::SystemState& st,
                                uint32_t nowMs,
                                bool doorOpenNow,
                                const char*& outFlag) const; // เดินลอจิก auto-arm แบบคาบเวลาเมื่อระบบอยู่โหมด disarm
};

} // namespace ApplicationLogicLayer
