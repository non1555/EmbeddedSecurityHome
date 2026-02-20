#!/usr/bin/env bash
set -euo pipefail

# Run LINE bridge + ngrok on Linux/Ubuntu.
# - Uses local venv Python
# - Reads HTTP_PORT from .env
# - Writes PID files (.bridge.pid/.ngrok.pid) for stop.sh

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
PY="$ROOT/.venv/bin/python3"
ENV_FILE="$ROOT/.env"
ENV_EXAMPLE="$ROOT/.env.example"
REQ_FILE="$ROOT/requirements.txt"

if [[ ! -x "$PY" ]]; then
  echo "Creating venv..."
  python3 -m venv "$ROOT/.venv"
fi

if [[ ! -f "$ENV_FILE" ]]; then
  if [[ -f "$ENV_EXAMPLE" ]]; then
    echo "Creating .env from .env.example..."
    cp -f "$ENV_EXAMPLE" "$ENV_FILE"
  fi
  echo "Please edit $ENV_FILE then run this again."
  exit 2
fi

if [[ ! -f "$REQ_FILE" ]]; then
  echo "ERROR: requirements.txt missing: $REQ_FILE"
  exit 2
fi

HTTP_PORT="$(grep -E '^HTTP_PORT=' "$ENV_FILE" | tail -n1 | cut -d= -f2- || true)"
HTTP_PORT="${HTTP_PORT:-8080}"

echo
echo "EmbeddedSecurity LINE Bridge Launcher (Linux)"
echo "============================================"
echo "Port: $HTTP_PORT"
echo

"$PY" -c "import fastapi,uvicorn,requests,paho.mqtt.client as mqtt" >/dev/null 2>&1 || {
  echo "Installing Python dependencies in venv..."
  "$PY" -m pip install -r "$REQ_FILE"
}

command -v ngrok >/dev/null 2>&1 || {
  echo "ERROR: ngrok not found in PATH."
  echo "Install ngrok and ensure 'ngrok' works in your terminal."
  exit 2
}

mkdir -p "$ROOT/logs"
TS="$(date +%Y%m%d-%H%M%S)"

# Free port (best-effort)
if command -v lsof >/dev/null 2>&1; then
  PIDS="$(lsof -tiTCP:"$HTTP_PORT" -sTCP:LISTEN || true)"
  if [[ -n "$PIDS" ]]; then
    echo "Port $HTTP_PORT is in use. Killing PID(s): $PIDS"
    kill -9 $PIDS || true
  fi
else
  echo "WARN: lsof not found; cannot auto-free port $HTTP_PORT"
fi

echo "Starting bridge..."
nohup "$PY" "$ROOT/bridge.py" >"$ROOT/logs/bridge-$TS.log" 2>&1 &
echo $! >"$ROOT/.bridge.pid"

echo "Starting ngrok..."
nohup ngrok http "$HTTP_PORT" >"$ROOT/logs/ngrok-$TS.log" 2>&1 &
echo $! >"$ROOT/.ngrok.pid"

sleep 2

NGROK_URL=""
if command -v curl >/dev/null 2>&1; then
  NGROK_URL="$(curl -fsS http://127.0.0.1:4040/api/tunnels 2>/dev/null | "$PY" -c "import json,sys; d=json.load(sys.stdin); ts=d.get('tunnels',[]); print(next((t.get('public_url','') for t in ts if t.get('proto')=='https'),''))" || true)"
fi

echo
echo "Health:"
echo "  http://127.0.0.1:$HTTP_PORT/health"
echo "ngrok inspector:"
echo "  http://127.0.0.1:4040"

if [[ -n "$NGROK_URL" ]]; then
  echo "ngrok public URL:"
  echo "  $NGROK_URL"
  echo "LINE webhook URL:"
  echo "  $NGROK_URL/line/webhook"
else
  echo "Could not read ngrok URL yet. Open http://127.0.0.1:4040 to copy the https URL."
fi

if command -v xdg-open >/dev/null 2>&1; then
  xdg-open "http://127.0.0.1:$HTTP_PORT/health" >/dev/null 2>&1 || true
  xdg-open "http://127.0.0.1:4040" >/dev/null 2>&1 || true
fi
