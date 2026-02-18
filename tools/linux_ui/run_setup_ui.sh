#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOG_DIR="$ROOT/logs/linux_ui"
mkdir -p "$LOG_DIR"
LOG_FILE="$LOG_DIR/setup-$(date +%Y%m%d-%H%M%S).log"

# shellcheck source=/dev/null
source "$SCRIPT_DIR/ui_common.sh"

if "$ROOT/setup.sh" >"$LOG_FILE" 2>&1; then
  ui_show_info "SecurityHome Setup" "Setup completed.\nLog: $LOG_FILE"
  ui_open_file "$LOG_FILE"
  exit 0
fi

ui_show_error "SecurityHome Setup Failed" "Setup failed.\nCheck log: $LOG_FILE"
ui_open_file "$LOG_FILE"
exit 1
