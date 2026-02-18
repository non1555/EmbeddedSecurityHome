#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOG_DIR="$ROOT/logs/linux_ui"
mkdir -p "$LOG_DIR"
LOG_FILE="$LOG_DIR/native-tests-$(date +%Y%m%d-%H%M%S).log"

# shellcheck source=/dev/null
source "$SCRIPT_DIR/ui_common.sh"

if "$ROOT/tools/run_native_flow_tests.sh" >"$LOG_FILE" 2>&1; then
  ui_show_info "Native Flow Tests" "Tests passed.\nLog: $LOG_FILE"
  ui_open_file "$LOG_FILE"
  exit 0
fi

ui_show_error "Native Flow Tests Failed" "Tests failed.\nCheck log: $LOG_FILE"
ui_open_file "$LOG_FILE"
exit 1
