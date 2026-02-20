# No-Board Simulator

This folder provides a host-side serial test simulator when no ESP32 board is available.

## Run

```bash
python tools/simulator/serial_test_simulator.py
```

Or run a quick scripted sequence:

```bash
python tools/simulator/serial_test_simulator.py 900 101 310 300 +16000 208
```

## Input

- numeric code: inject event (`100`, `310`, `320`, etc.)
- `?` or `help`: show code list
- `+<ms>`: advance simulated time (for decay/timeout checks)
- `exit`: quit

## Output

Each injected code prints separate lines:

- event
- command
- mode/level/score
- output lock/open state
- line notification kind (debug only)
- LINE message preview (`[TRACE] line.message=...`)

Note: end-user alerts should be delivered via LINE bridge, not Serial text output.
