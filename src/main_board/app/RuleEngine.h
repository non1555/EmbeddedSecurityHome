#pragma once
#include "SystemState.h"
#include "Events.h"
#include "Commands.h"
#include "Config.h"

struct Decision {
  SystemState next;
  Command cmd;
};

class RuleEngine {
public:
  Decision handle(const SystemState& s, const Config& cfg, const Event& e) const;
};
