#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOG_DIR="$ROOT/logs/linux_ui"
mkdir -p "$LOG_DIR"
LOG_FILE="$LOG_DIR/launcher-ui-$(date +%Y%m%d-%H%M%S).log"

# shellcheck source=/dev/null
source "$SCRIPT_DIR/ui_common.sh"

BOOTSTRAP="$ROOT/tools/line_bridge/bootstrap.sh"
if [[ ! -x "$BOOTSTRAP" ]]; then
  chmod +x "$BOOTSTRAP" 2>/dev/null || true
fi
if [[ ! -x "$BOOTSTRAP" ]]; then
  ui_show_error "Launcher UI" "Missing bootstrap script:\n$BOOTSTRAP"
  exit 2
fi

if ! "$BOOTSTRAP" --prepare-only >"$LOG_FILE" 2>&1; then
  ui_show_error "Launcher UI" "Prepare failed.\nCheck log: $LOG_FILE"
  ui_open_file "$LOG_FILE"
  exit 1
fi

VPY="$ROOT/tools/line_bridge/.venv/bin/python3"
LAUNCHER="$ROOT/tools/line_bridge/launcher.pyw"
SETUP_SCRIPT="$ROOT/setup.sh"
if [[ ! -x "$VPY" ]]; then
  ui_show_error "Launcher UI" "Python venv not found:\n$VPY"
  exit 2
fi
if [[ ! -f "$LAUNCHER" ]]; then
  ui_show_error "Launcher UI" "Launcher not found:\n$LAUNCHER"
  exit 2
fi

if ! "$VPY" -c "import tkinter" >/dev/null 2>&1; then
  if [[ ! -x "$SETUP_SCRIPT" ]]; then
    chmod +x "$SETUP_SCRIPT" 2>/dev/null || true
  fi
  if [[ ! -x "$SETUP_SCRIPT" ]]; then
    ui_show_error "Launcher UI" "tkinter is missing and setup script is unavailable:\n$SETUP_SCRIPT\n\nLog: $LOG_FILE"
    ui_open_file "$LOG_FILE"
    exit 2
  fi
  ui_show_info "Launcher UI" "tkinter is missing.\nRunning setup.sh to install required dependencies."
  if ! "$SETUP_SCRIPT" >>"$LOG_FILE" 2>&1; then
    ui_show_error "Launcher UI" "setup.sh failed while installing dependencies.\nCheck log: $LOG_FILE"
    ui_open_file "$LOG_FILE"
    exit 2
  fi
  if ! "$VPY" -c "import tkinter" >/dev/null 2>&1; then
    ui_show_error "Launcher UI" "tkinter is still missing after setup.\nInstall with:\nsudo apt-get install -y python3-tk\n\nLog: $LOG_FILE"
    ui_open_file "$LOG_FILE"
    exit 2
  fi
fi

nohup "$VPY" "$LAUNCHER" >>"$LOG_FILE" 2>&1 &
pid=$!
sleep 1
if ! kill -0 "$pid" >/dev/null 2>&1; then
  ui_show_error "Launcher UI" "Launcher failed to start.\nCheck log: $LOG_FILE"
  ui_open_file "$LOG_FILE"
  exit 1
fi

exit 0
