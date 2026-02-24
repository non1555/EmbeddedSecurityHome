#pragma once

#include "configuration_shared_types/Types.h"

namespace ApplicationLogicLayer {

class SystemContext;

class RuleEngine {
public:
  void process(const ConfigurationSharedTypes::Event& e, SystemContext& context) const; // Processes one event end-to-end using context state and side effects. Params: e=input event, context=system context object.
  ConfigurationSharedTypes::Decision handle(const ConfigurationSharedTypes::SystemState& st,
                                            const ConfigurationSharedTypes::Event& e) const; // Evaluates one event against current state-machine rules. Params: st=current state snapshot, e=input event.

  bool processDisarmAutoArmTick(ConfigurationSharedTypes::SystemState& st,
                                uint32_t nowMs,
                                bool doorOpenNow,
                                const char*& outFlag) const; // Runs periodic auto-arm progression while in disarm mode. Params: st=mutable state, nowMs=current timestamp in ms, doorOpenNow=current door-open status, outFlag=result tag string.
};

} // namespace ApplicationLogicLayer
