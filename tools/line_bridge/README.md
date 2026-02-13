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

```bash
cd tools/line_bridge
copy .env.example .env
```

Edit `.env` values.

Important fields:
- `MQTT_BROKER`, `MQTT_PORT`, `MQTT_USERNAME`, `MQTT_PASSWORD`
- `LINE_CHANNEL_ACCESS_TOKEN`, `LINE_CHANNEL_SECRET`
- one target: `LINE_TARGET_USER_ID` or `LINE_TARGET_GROUP_ID` or `LINE_TARGET_ROOM_ID`
- metrics throttle: `METRICS_PUSH_PERIOD_S` (default 30s)

## Broker on this machine

Detected on this PC:
- Mosquitto installed at `C:\Program Files\mosquitto\mosquitto.exe`
- Listener config `listener 1883 0.0.0.0` in `C:\Program Files\mosquitto\mosquitto.conf`
- LAN IP used for ESP32: `192.168.1.58`

If broker is not running, start it:

```powershell
& "C:\Program Files\mosquitto\mosquitto.exe" -c "C:\Program Files\mosquitto\mosquitto.conf" -v
```

## 3) Install and run

```bash
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python bridge.py
```

Health check:
- `GET http://127.0.0.1:8080/health`

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
