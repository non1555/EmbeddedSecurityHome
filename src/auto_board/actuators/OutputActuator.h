#pragma once

namespace OutputActuator {

struct State {
  bool lightOn;
  bool fanOn;
};

void init();
void apply(const State& state);

} // namespace OutputActuator
