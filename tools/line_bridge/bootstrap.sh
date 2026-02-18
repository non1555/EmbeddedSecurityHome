#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREP_ONLY=0
if [[ "${1:-}" == "--prepare-only" ]]; then
  PREP_ONLY=1
fi

ENV_FILE="$ROOT/.env"
ENV_EXAMPLE="$ROOT/.env.example"
if [[ ! -f "$ENV_FILE" && -f "$ENV_EXAMPLE" ]]; then
  echo "Creating .env from .env.example..."
  cp -f "$ENV_EXAMPLE" "$ENV_FILE"
fi

generate_cmd_token() {
  if command -v openssl >/dev/null 2>&1; then
    openssl rand -hex 16
    return 0
  fi
  if command -v od >/dev/null 2>&1; then
    od -An -N16 -tx1 /dev/urandom | tr -d ' \n'
    return 0
  fi
  tr -dc 'A-Za-z0-9' </dev/urandom | head -c 32
}

ensure_cmd_token() {
  if [[ ! -f "$ENV_FILE" ]]; then
    return
  fi
  local current=""
  current="$(grep -E '^FW_CMD_TOKEN=' "$ENV_FILE" | tail -n1 | cut -d= -f2- | tr -d '\r\n[:space:]' || true)"
  if [[ -n "$current" ]]; then
    return
  fi
  local token
  token="$(generate_cmd_token)"
  if [[ -z "$token" ]]; then
    echo "WARN: unable to generate FW_CMD_TOKEN automatically."
    return
  fi
  if grep -q '^FW_CMD_TOKEN=' "$ENV_FILE"; then
    sed -i "s/^FW_CMD_TOKEN=.*/FW_CMD_TOKEN=$token/" "$ENV_FILE"
  else
    printf '\nFW_CMD_TOKEN=%s\n' "$token" >>"$ENV_FILE"
  fi
  echo "Generated FW_CMD_TOKEN in .env"
}

ensure_cmd_token

PY=""
if command -v python3 >/dev/null 2>&1; then
  PY="python3"
elif command -v python >/dev/null 2>&1; then
  PY="python"
else
  echo "ERROR: Python not found. Install Python 3.10+ first."
  exit 2
fi

VPY="$ROOT/.venv/bin/python3"
if [[ ! -x "$VPY" ]]; then
  echo "Creating virtual environment..."
  "$PY" -m venv "$ROOT/.venv"
fi

echo "Installing bridge dependencies..."
"$VPY" -m pip install --upgrade pip
"$VPY" -m pip install -r "$ROOT/requirements.txt"

if ! "$VPY" -c "import tkinter" >/dev/null 2>&1; then
  echo "WARN: tkinter is missing. Install python3-tk to use launcher UI."
fi

if ! command -v ngrok >/dev/null 2>&1; then
  if [[ -x "$ROOT/../ngrok/ngrok" ]]; then
    export PATH="$ROOT/../ngrok:$PATH"
  fi
fi
if ! command -v ngrok >/dev/null 2>&1; then
  echo "WARN: ngrok not found. Install from https://ngrok.com/downloads"
fi

if [[ "$PREP_ONLY" -eq 1 ]]; then
  exit 0
fi

nohup "$VPY" "$ROOT/launcher.pyw" >/dev/null 2>&1 &
echo "Launcher started."
