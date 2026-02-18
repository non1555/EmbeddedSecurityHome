#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
APP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
DESKTOP_DIR="$HOME/Desktop"

mkdir -p "$APP_DIR"

write_launcher() {
  local file_name="$1"
  local label="$2"
  local exec_path="$3"
  local icon_name="$4"
  local target="$APP_DIR/$file_name"
  cat >"$target" <<EOF_DESKTOP
[Desktop Entry]
Type=Application
Version=1.0
Name=$label
Exec=$exec_path
Path=$ROOT
Terminal=false
Icon=$icon_name
Categories=Utility;
StartupNotify=true
EOF_DESKTOP
  chmod +x "$target"
}

write_launcher "securityhome-setup.desktop" \
  "SecurityHome Launcher" \
  "$ROOT/tools/linux_ui/open_launcher_ui.sh" \
  "applications-system"

write_launcher "securityhome-line-bridge-start.desktop" \
  "SecurityHome LINE Start" \
  "$ROOT/tools/linux_ui/start_line_bridge_ui.sh" \
  "media-playback-start"

write_launcher "securityhome-line-bridge-stop.desktop" \
  "SecurityHome LINE Stop" \
  "$ROOT/tools/linux_ui/stop_line_bridge_ui.sh" \
  "media-playback-stop"

write_launcher "securityhome-native-tests.desktop" \
  "SecurityHome Native Tests" \
  "$ROOT/tools/linux_ui/run_native_tests_ui.sh" \
  "utilities-terminal"

if [[ -d "$DESKTOP_DIR" ]]; then
  cp -f "$APP_DIR"/securityhome-*.desktop "$DESKTOP_DIR"/
  chmod +x "$DESKTOP_DIR"/securityhome-*.desktop
fi

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi

echo "Installed launchers to: $APP_DIR"
if [[ -d "$DESKTOP_DIR" ]]; then
  echo "Copied launchers to desktop: $DESKTOP_DIR"
fi
