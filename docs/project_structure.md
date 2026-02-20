# Project Structure Guide

## Root (`/`)

Keep only project-level files:

- build and environment config: `platformio.ini`, `platformio.local.ini`
- setup and launch entrypoints: `setup.cmd`, `setup.sh`, `*.desktop`
- top-level project readme: `README.md`
- pin quick-reference: `PIN_PLAN.txt`

Do not keep temporary diagram files (`tmp_*`) at root.

## Source (`src/`)

- `src/main_board/`: active firmware target and all runtime logic
- `src/auto_board/`: legacy archive (not part of active build/test flow)

## Documentation (`docs/`)

- final active docs: diagrams, rules, requirements, test mapping
- `legacy_2026-02-19/`: archived historical multi-board docs (read-only archive)
- `submission_pack/`: submission checklist and evidence templates
- `workbench/`: temporary drafts and scratch artifacts

## Tooling (`tools/`)

- `tools/line_bridge/`: LINE bridge runtime and launcher
- `tools/linux_ui/`: Linux desktop launcher helpers
- `tools/ngrok/`: tunnel binary/storage
- `tools/simulator/`: no-board serial/input simulation tools
- helper scripts: `tools/pio_env.py`, `tools/run_native_flow_tests.sh`

## Tests (`test/`)

- `test/native_flow/`: native firmware flow tests
- `test/bridge/`: line bridge tests
- `test/stubs/`: host-side stubs
