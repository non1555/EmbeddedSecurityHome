# MQTT <-> LINE Bridge

This bridge connects your ESP32 MQTT topics to LINE Official Account:

- MQTT `esh/event`, `esh/status`, `esh/ack` -> push message to LINE
- MQTT `esh/metrics` -> summarized push to LINE (throttled)
- LINE webhook text command -> publish to MQTT `esh/cmd`

## 1) Prepare LINE OA

1. Create LINE Official Account + Messaging API channel.
2. Get:
   - `LINE_CHANNEL_ACCESS_TOKEN` (long-lived token)
   - `LINE_CHANNEL_SECRET`
3. Set webhook URL to:
   - `https://<your-public-host>/line/webhook`
4. Add your OA as friend and get one target ID:
   - user ID (`U...`) or group ID (`C...`) or room ID (`R...`)

## 2) Setup bridge env

Windows:
```bat
cd tools\line_bridge
copy .env.example .env
```

Linux/Ubuntu:
```bash
cd tools/line_bridge
cp .env.example .env
```

Edit `.env` values.

Important fields:
- `MQTT_BROKER`, `MQTT_PORT`, `MQTT_USERNAME`, `MQTT_PASSWORD`
- `LINE_CHANNEL_ACCESS_TOKEN`, `LINE_CHANNEL_SECRET`
- `NGROK_AUTHTOKEN` (required)
- optional target (for push pinning): `LINE_TARGET_USER_ID` or `LINE_TARGET_GROUP_ID` or `LINE_TARGET_ROOM_ID`
- metrics throttle: `METRICS_PUSH_PERIOD_S` (default 30s)

Notes:
- If you do not set any `LINE_TARGET_*`, the bridge will automatically learn a push target from the first LINE webhook it receives (send any message in the chat you want to receive pushes to).

## Broker on this machine
Run an MQTT broker reachable by both:
- ESP32 firmware (`MQTT_BROKER` in `platformio.ini`)
- this bridge (`MQTT_BROKER` in `.env`)

## 3) Install and run

Windows (CLI):
```bat
cd tools\line_bridge
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt
.venv\Scripts\python.exe bridge.py
```

Linux/Ubuntu (CLI):
```bash
cd tools/line_bridge
python3 -m venv .venv
.venv/bin/python3 -m pip install -r requirements.txt
.venv/bin/python3 bridge.py
```

Convenience scripts:
- Windows: `tools\line_bridge\run.cmd` and `tools\line_bridge\stop.cmd`
- Linux/Ubuntu: `tools/line_bridge/run.sh` and `tools/line_bridge/stop.sh` (run `chmod +x tools/line_bridge/*.sh` once)

UI Launcher (no terminal):
- Double-click `tools/line_bridge/launcher.vbs`
- Click `Start`, then use `Copy Webhook` and paste into LINE Developers as `.../line/webhook`
- Control is handled in LINE chat (Rich Menu + Flex UI).
- Firmware UI: use the `Firmware` tab to set ESP Wi-Fi/MQTT values, then click `Build`/`Upload`.
- Launcher saves your values in `tools/line_bridge/.env`.
  - `platformio.ini` reads `.env` via `tools/pio_env.py` (PlatformIO pre-build script).
  - `platformio.ini` is not edited by launcher.

## Rich Menu (Bottom Bar)

LINE "Rich menu" (the bottom bar UI) requires an image. This project generates one for you.

1. Ensure your `.env` has `LINE_CHANNEL_ACCESS_TOKEN`.
2. Install Pillow (one-time):
   - Windows: `tools\\line_bridge\\.venv\\Scripts\\python.exe -m pip install pillow`
   - Linux: `tools/line_bridge/.venv/bin/python3 -m pip install pillow`
3. Create + set default rich menu:
   - Windows: `tools\\line_bridge\\.venv\\Scripts\\python.exe tools\\line_bridge\\richmenu_setup.py`
   - Linux: `tools/line_bridge/.venv/bin/python3 tools/line_bridge/richmenu_setup.py`

The bottom bar buttons open the in-chat Flex UI (`Mode` / `Lock`).
`richmenu_setup.py` now regenerates `richmenu.png` by default every run.
If you want to keep your own image file, run with `--use-existing-image`.

UI Launcher (Linux/Ubuntu):
- Install Tkinter if missing: `sudo apt-get install -y python3-tk`
- Run: `tools/line_bridge/.venv/bin/python3 tools/line_bridge/launcher.pyw`

Port:
- Default is `HTTP_PORT=8080` (see `.env`). If you get Windows error `WinError 10048`, another process is already using the port. Either stop that process or change `HTTP_PORT` (e.g. 8081) and restart the bridge.

Health check:
- `GET http://127.0.0.1:<HTTP_PORT>/health`

`/health` returns:
- `mqtt_connected`: whether the bridge is connected to MQTT
- `line_webhook_ready`: token+secret configured (webhook verify + replies)
- `line_push_ready`: token+target configured (push notifications)
- `ready`: true when MQTT + LINE webhook + LINE push are all ready
- `problems`: list of missing pieces when not ready

## 4) Supported LINE commands

- `status`
- `lock door`
- `unlock door`
- `lock window`
- `unlock window`
- `lock all`
- `unlock all`
- `help`

These commands are published to MQTT topic `esh/cmd`.

## 5) Topic contract (from firmware)

- `esh/cmd` (subscribe by ESP32): plain text command
- `esh/event` (publish by ESP32): JSON event snapshot
- `esh/status` (publish by ESP32): JSON status snapshot
- `esh/ack` (publish by ESP32): JSON command ack
- `esh/metrics` (publish by ESP32): JSON metrics (bridge forwards summary)
