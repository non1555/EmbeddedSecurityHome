#!/usr/bin/env bash
set -euo pipefail

# Stop LINE bridge + ngrok on Linux/Ubuntu (best-effort).

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$ROOT/.env"

HTTP_PORT="8080"
if [[ -f "$ENV_FILE" ]]; then
  HTTP_PORT="$(grep -E '^HTTP_PORT=' "$ENV_FILE" | tail -n1 | cut -d= -f2- || true)"
  HTTP_PORT="${HTTP_PORT:-8080}"
fi

echo "Stopping bridge on port $HTTP_PORT..."

if [[ -f "$ROOT/.bridge.pid" ]]; then
  PID="$(cat "$ROOT/.bridge.pid" || true)"
  if [[ -n "${PID:-}" ]]; then
    kill -9 "$PID" >/dev/null 2>&1 || true
  fi
  rm -f "$ROOT/.bridge.pid"
fi

echo "Stopping ngrok..."
if [[ -f "$ROOT/.ngrok.pid" ]]; then
  PID="$(cat "$ROOT/.ngrok.pid" || true)"
  if [[ -n "${PID:-}" ]]; then
    kill -9 "$PID" >/dev/null 2>&1 || true
  fi
  rm -f "$ROOT/.ngrok.pid"
fi

# Free port (best-effort)
if command -v lsof >/dev/null 2>&1; then
  PIDS="$(lsof -tiTCP:"$HTTP_PORT" -sTCP:LISTEN || true)"
  if [[ -n "$PIDS" ]]; then
    kill -9 $PIDS >/dev/null 2>&1 || true
  fi
fi

echo "Stopped."

