#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOOTSTRAP="$ROOT/tools/line_bridge/bootstrap.sh"
ENV_FILE="$ROOT/tools/line_bridge/.env"
NGROK_BUNDLED="$ROOT/tools/ngrok/ngrok"
HAS_PIO=0
HAS_NGROK=0

echo "[1/4] Bootstrap bridge environment..."
if [[ ! -x "$BOOTSTRAP" ]]; then
  chmod +x "$BOOTSTRAP" 2>/dev/null || true
fi
if [[ ! -x "$BOOTSTRAP" ]]; then
  echo "ERROR: Missing bootstrap script: $BOOTSTRAP"
  exit 2
fi
"$BOOTSTRAP" --prepare-only

echo "[2/4] Check PlatformIO..."
if command -v platformio >/dev/null 2>&1 || command -v pio >/dev/null 2>&1; then
  HAS_PIO=1
elif command -v python3 >/dev/null 2>&1; then
  if python3 -m platformio --version >/dev/null 2>&1; then
    HAS_PIO=1
  else
    echo "Installing PlatformIO via pip..."
    python3 -m pip install --upgrade platformio
    python3 -m platformio --version >/dev/null 2>&1 && HAS_PIO=1 || true
  fi
elif command -v python >/dev/null 2>&1; then
  if python -m platformio --version >/dev/null 2>&1; then
    HAS_PIO=1
  else
    echo "Installing PlatformIO via pip..."
    python -m pip install --upgrade platformio
    python -m platformio --version >/dev/null 2>&1 && HAS_PIO=1 || true
  fi
fi

echo "[3/4] Check ngrok..."
if command -v ngrok >/dev/null 2>&1; then
  HAS_NGROK=1
elif [[ -x "$NGROK_BUNDLED" ]]; then
  HAS_NGROK=1
fi

echo "[4/4] Verify required local files..."
HAS_ENV=0
[[ -f "$ENV_FILE" ]] && HAS_ENV=1

echo
echo "===== Setup Summary ====="
if [[ "$HAS_PIO" -eq 1 ]]; then
  echo "PlatformIO: OK"
else
  echo "PlatformIO: MISSING (install Python then run: python3 -m pip install platformio)"
fi
if [[ "$HAS_NGROK" -eq 1 ]]; then
  echo "ngrok: OK"
else
  echo "ngrok: MISSING (install from https://ngrok.com/downloads)"
fi
if [[ "$HAS_ENV" -eq 1 ]]; then
  echo "tools/line_bridge/.env: OK"
else
  echo "tools/line_bridge/.env: MISSING"
fi
echo
echo "Next:"
echo "1) Fill LINE/ngrok values in tools/line_bridge/.env"
echo "2) Launch UI: tools/line_bridge/.venv/bin/python3 tools/line_bridge/launcher.pyw"
echo "3) Firmware build: python3 -m platformio run"

if [[ "$HAS_PIO" -ne 1 || "$HAS_NGROK" -ne 1 ]]; then
  exit 1
fi
exit 0
