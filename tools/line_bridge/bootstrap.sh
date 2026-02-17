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
