#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p .pio/native

CXX_BIN="${CXX:-g++}"

"$CXX_BIN" -std=c++17 -Wall -Wextra -Wno-unused-function -pedantic \
  -Itest/stubs -Isrc -Isrc/main_board \
  test/native_flow/test_main.cpp \
  src/main_board/app/RuleEngine.cpp \
  -o .pio/native/native_flow_tests

.pio/native/native_flow_tests
