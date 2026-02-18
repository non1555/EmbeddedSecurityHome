#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOOTSTRAP="$ROOT/tools/line_bridge/bootstrap.sh"
ENV_FILE="$ROOT/tools/line_bridge/.env"
NGROK_BUNDLED="$ROOT/tools/ngrok/ngrok"
PIO_VENV="$ROOT/.venv_pio"
PIO_VENV_PY="$PIO_VENV/bin/python3"
PIO_VENV_PIO="$PIO_VENV/bin/pio"
HAS_PIO=0
HAS_NGROK=0
HAS_TK=0
HAS_MOSQ=0
MOSQ_RUNNING=0

run_privileged() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
    return $?
  fi
  # Launcher/UI path usually has no TTY; prefer pkexec for GUI auth prompt.
  if [[ ! -t 0 ]] && command -v pkexec >/dev/null 2>&1; then
    pkexec "$@"
    return $?
  fi
  if command -v sudo >/dev/null 2>&1; then
    sudo "$@"
    return $?
  fi
  if command -v pkexec >/dev/null 2>&1; then
    pkexec "$@"
    return $?
  fi
  echo "ERROR: Root privilege is required, but neither sudo nor pkexec is available."
  return 1
}

detect_python() {
  if command -v python3 >/dev/null 2>&1; then
    echo "python3"
    return
  fi
  if command -v python >/dev/null 2>&1; then
    echo "python"
    return
  fi
  echo ""
}

echo "[1/6] Bootstrap bridge environment..."
if [[ ! -x "$BOOTSTRAP" ]]; then
  chmod +x "$BOOTSTRAP" 2>/dev/null || true
fi
if [[ ! -x "$BOOTSTRAP" ]]; then
  echo "ERROR: Missing bootstrap script: $BOOTSTRAP"
  exit 2
fi
"$BOOTSTRAP" --prepare-only

echo "[2/6] Check PlatformIO..."
if [[ -x "$PIO_VENV_PIO" ]] && "$PIO_VENV_PIO" --version >/dev/null 2>&1; then
  HAS_PIO=1
elif command -v platformio >/dev/null 2>&1 || command -v pio >/dev/null 2>&1; then
  HAS_PIO=1
elif command -v python3 >/dev/null 2>&1; then
  if python3 -m platformio --version >/dev/null 2>&1; then
    HAS_PIO=1
  else
    echo "Installing PlatformIO into project venv (.venv_pio)..."
    if [[ ! -x "$PIO_VENV_PY" ]]; then
      python3 -m venv "$PIO_VENV"
    fi
    "$PIO_VENV_PY" -m pip install --upgrade pip
    "$PIO_VENV_PY" -m pip install --upgrade platformio
    "$PIO_VENV_PIO" --version >/dev/null 2>&1 && HAS_PIO=1 || true
  fi
elif command -v python >/dev/null 2>&1; then
  if python -m platformio --version >/dev/null 2>&1; then
    HAS_PIO=1
  else
    echo "Installing PlatformIO into project venv (.venv_pio)..."
    if [[ ! -x "$PIO_VENV_PY" ]]; then
      python -m venv "$PIO_VENV"
    fi
    "$PIO_VENV_PY" -m pip install --upgrade pip
    "$PIO_VENV_PY" -m pip install --upgrade platformio
    "$PIO_VENV_PIO" --version >/dev/null 2>&1 && HAS_PIO=1 || true
  fi
fi

echo "[3/6] Check ngrok..."
if command -v ngrok >/dev/null 2>&1; then
  HAS_NGROK=1
elif [[ -x "$NGROK_BUNDLED" ]]; then
  HAS_NGROK=1
fi

echo "[4/6] Check tkinter (python3-tk)..."
PY_BIN="$(detect_python)"
if [[ -n "$PY_BIN" ]] && "$PY_BIN" -c "import tkinter" >/dev/null 2>&1; then
  HAS_TK=1
else
  if command -v apt-get >/dev/null 2>&1; then
    echo "Installing python3-tk (requires admin authentication)..."
    if run_privileged apt-get install -y python3-tk >/dev/null 2>&1; then
      if [[ -n "$PY_BIN" ]] && "$PY_BIN" -c "import tkinter" >/dev/null 2>&1; then
        HAS_TK=1
      fi
    fi
  fi
fi

echo "[5/6] Check mosquitto..."
if command -v mosquitto >/dev/null 2>&1; then
  HAS_MOSQ=1
else
  if command -v apt-get >/dev/null 2>&1; then
    echo "Installing mosquitto (requires admin authentication)..."
    if run_privileged apt-get install -y mosquitto >/dev/null 2>&1; then
      command -v mosquitto >/dev/null 2>&1 && HAS_MOSQ=1 || true
    fi
  fi
fi

if [[ "$HAS_MOSQ" -eq 1 ]] && command -v systemctl >/dev/null 2>&1; then
  if run_privileged systemctl enable --now mosquitto >/dev/null 2>&1; then
    systemctl is-active --quiet mosquitto && MOSQ_RUNNING=1 || true
  fi
fi

echo "[6/6] Verify required local files..."
HAS_ENV=0
[[ -f "$ENV_FILE" ]] && HAS_ENV=1

echo
echo "===== Setup Summary ====="
if [[ "$HAS_PIO" -eq 1 ]]; then
  echo "PlatformIO: OK"
else
  echo "PlatformIO: MISSING (install in project venv with: python3 -m venv .venv_pio && .venv_pio/bin/python3 -m pip install platformio)"
fi
if [[ "$HAS_NGROK" -eq 1 ]]; then
  echo "ngrok: OK"
else
  echo "ngrok: MISSING (install from https://ngrok.com/downloads)"
fi
if [[ "$HAS_TK" -eq 1 ]]; then
  echo "python3-tk: OK"
else
  echo "python3-tk: MISSING (install with: sudo apt-get install -y python3-tk)"
fi
if [[ "$HAS_MOSQ" -eq 1 ]]; then
  if [[ "$MOSQ_RUNNING" -eq 1 ]]; then
    echo "mosquitto: OK (running)"
  else
    echo "mosquitto: OK (installed, service may not be running)"
  fi
else
  echo "mosquitto: MISSING (install with: sudo apt-get install -y mosquitto)"
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
if [[ "$HAS_TK" -ne 1 || "$HAS_MOSQ" -ne 1 ]]; then
  exit 1
fi
exit 0
