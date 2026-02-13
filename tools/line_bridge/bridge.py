import base64
import hashlib
import hmac
import json
import os
import threading
import time
from typing import Any, Dict, Optional

import paho.mqtt.client as mqtt
import requests
from fastapi import FastAPI, Header, HTTPException, Request


def load_env_file(path: str) -> None:
    if not os.path.exists(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, value = line.split("=", 1)
            key = key.strip()
            value = value.strip()
            if key and key not in os.environ:
                os.environ[key] = value


def env(name: str, default: str = "") -> str:
    return os.environ.get(name, default)


load_env_file(".env")

MQTT_BROKER = env("MQTT_BROKER", "127.0.0.1")
MQTT_PORT = int(env("MQTT_PORT", "1883"))
MQTT_USERNAME = env("MQTT_USERNAME")
MQTT_PASSWORD = env("MQTT_PASSWORD")
MQTT_CLIENT_ID = env("MQTT_CLIENT_ID", "esh-line-bridge")
MQTT_TOPIC_CMD = env("MQTT_TOPIC_CMD", "esh/cmd")
MQTT_TOPIC_EVENT = env("MQTT_TOPIC_EVENT", "esh/event")
MQTT_TOPIC_STATUS = env("MQTT_TOPIC_STATUS", "esh/status")
MQTT_TOPIC_ACK = env("MQTT_TOPIC_ACK", "esh/ack")
MQTT_TOPIC_METRICS = env("MQTT_TOPIC_METRICS", "esh/metrics")
METRICS_PUSH_PERIOD_S = max(5, int(env("METRICS_PUSH_PERIOD_S", "30")))

LINE_CHANNEL_ACCESS_TOKEN = env("LINE_CHANNEL_ACCESS_TOKEN")
LINE_CHANNEL_SECRET = env("LINE_CHANNEL_SECRET")
LINE_TARGET_USER_ID = env("LINE_TARGET_USER_ID")
LINE_TARGET_GROUP_ID = env("LINE_TARGET_GROUP_ID")
LINE_TARGET_ROOM_ID = env("LINE_TARGET_ROOM_ID")

HTTP_HOST = env("HTTP_HOST", "0.0.0.0")
HTTP_PORT = int(env("HTTP_PORT", "8080"))


def get_line_target() -> Optional[str]:
    if LINE_TARGET_USER_ID:
        return LINE_TARGET_USER_ID
    if LINE_TARGET_GROUP_ID:
        return LINE_TARGET_GROUP_ID
    if LINE_TARGET_ROOM_ID:
        return LINE_TARGET_ROOM_ID
    return None


def parse_json_payload(payload: str) -> Dict[str, Any]:
    try:
        obj = json.loads(payload)
        if isinstance(obj, dict):
            return obj
    except Exception:
        pass
    return {}


def format_mqtt_to_text(topic: str, payload: str) -> str:
    obj = parse_json_payload(payload)
    if topic == MQTT_TOPIC_EVENT:
        return (
            "[EVENT]\n"
            f"event={obj.get('event', '-')}\n"
            f"src={obj.get('src', '-')}\n"
            f"cmd={obj.get('cmd', '-')}\n"
            f"mode={obj.get('mode', '-')}\n"
            f"level={obj.get('level', '-')}"
        )
    if topic == MQTT_TOPIC_STATUS:
        return (
            "[STATUS]\n"
            f"reason={obj.get('reason', '-')}\n"
            f"mode={obj.get('mode', '-')}\n"
            f"level={obj.get('level', '-')}\n"
            f"uptime_ms={obj.get('uptime_ms', '-')}"
        )
    if topic == MQTT_TOPIC_ACK:
        return (
            "[ACK]\n"
            f"cmd={obj.get('cmd', '-')}\n"
            f"ok={obj.get('ok', '-')}\n"
            f"detail={obj.get('detail', '-')}"
        )
    return f"[MQTT]\ntopic={topic}\npayload={payload}"


def line_api_headers() -> Dict[str, str]:
    return {
        "Authorization": f"Bearer {LINE_CHANNEL_ACCESS_TOKEN}",
        "Content-Type": "application/json",
    }


def push_line_text(text: str) -> None:
    target = get_line_target()
    if not LINE_CHANNEL_ACCESS_TOKEN or not target:
        return
    body = {
        "to": target,
        "messages": [{"type": "text", "text": text[:4800]}],
    }
    requests.post(
        "https://api.line.me/v2/bot/message/push",
        headers=line_api_headers(),
        json=body,
        timeout=8,
    )


def reply_line_text(reply_token: str, text: str) -> None:
    if not LINE_CHANNEL_ACCESS_TOKEN:
        return
    body = {
        "replyToken": reply_token,
        "messages": [{"type": "text", "text": text[:4800]}],
    }
    requests.post(
        "https://api.line.me/v2/bot/message/reply",
        headers=line_api_headers(),
        json=body,
        timeout=8,
    )


class BridgeState:
    def __init__(self) -> None:
        self.mqtt_connected = False
        self.last_mqtt_rx_topic = ""
        self.last_mqtt_rx_payload = ""
        self.last_mqtt_rx_at = 0.0
        self.last_cmd = ""
        self.last_cmd_at = 0.0
        self.last_metrics_push_at = 0.0


state = BridgeState()
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=MQTT_CLIENT_ID, clean_session=True)


def publish_cmd(cmd: str) -> None:
    text = cmd.strip().lower()
    if not text:
        return
    mqtt_client.publish(MQTT_TOPIC_CMD, payload=text, qos=0, retain=False)
    state.last_cmd = text
    state.last_cmd_at = time.time()


def is_supported_cmd(text: str) -> bool:
    allowed = {
        "status",
        "lock door",
        "unlock door",
        "lock window",
        "unlock window",
        "lock all",
        "unlock all",
    }
    return text in allowed


def verify_line_signature(raw_body: bytes, signature: str) -> bool:
    if not LINE_CHANNEL_SECRET:
        return False
    digest = hmac.new(
        LINE_CHANNEL_SECRET.encode("utf-8"),
        raw_body,
        hashlib.sha256,
    ).digest()
    expected = base64.b64encode(digest).decode("utf-8")
    return hmac.compare_digest(expected, signature)


def on_connect(client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any) -> None:
    state.mqtt_connected = (reason_code == 0)
    client.subscribe(
        [
            (MQTT_TOPIC_EVENT, 0),
            (MQTT_TOPIC_STATUS, 0),
            (MQTT_TOPIC_ACK, 0),
            (MQTT_TOPIC_METRICS, 0),
        ]
    )
    push_line_text("[BRIDGE] connected to MQTT")


def on_disconnect(client: mqtt.Client, userdata: Any, disconnect_flags: Any, reason_code: Any, properties: Any) -> None:
    state.mqtt_connected = False
    push_line_text(f"[BRIDGE] disconnected from MQTT rc={reason_code}")


def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
    payload = msg.payload.decode("utf-8", errors="replace")
    topic = msg.topic
    state.last_mqtt_rx_topic = topic
    state.last_mqtt_rx_payload = payload
    state.last_mqtt_rx_at = time.time()
    if topic == MQTT_TOPIC_METRICS:
      now = time.time()
      if (now - state.last_metrics_push_at) < METRICS_PUSH_PERIOD_S:
          return
      state.last_metrics_push_at = now
      obj = parse_json_payload(payload)
      push_line_text(
          "[METRICS]\n"
          f"us_drops={obj.get('us_drops', '-')}\n"
          f"pub_drops={obj.get('pub_drops', '-')}\n"
          f"cmd_drops={obj.get('cmd_drops', '-')}\n"
          f"store_drops={obj.get('store_drops', '-')}\n"
          f"q_us={obj.get('q_us', '-')}\n"
          f"q_pub={obj.get('q_pub', '-')}\n"
          f"q_cmd={obj.get('q_cmd', '-')}\n"
          f"q_store={obj.get('q_store', '-')}"
      )
      return
    push_line_text(format_mqtt_to_text(topic, payload))


mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_message = on_message


def mqtt_loop_thread() -> None:
    if MQTT_USERNAME:
        mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    while True:
        try:
            mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=20)
            mqtt_client.loop_forever(retry_first_connection=True)
        except Exception:
            state.mqtt_connected = False
            time.sleep(3)


app = FastAPI(title="EmbeddedSecurity LINE Bridge")


@app.get("/health")
def health() -> Dict[str, Any]:
    return {
        "ok": True,
        "mqtt_connected": state.mqtt_connected,
        "last_mqtt_topic": state.last_mqtt_rx_topic,
        "last_cmd": state.last_cmd,
    }


@app.post("/line/webhook")
async def line_webhook(
    request: Request,
    x_line_signature: str = Header(default=""),
) -> Dict[str, Any]:
    body = await request.body()
    if not verify_line_signature(body, x_line_signature):
        raise HTTPException(status_code=401, detail="invalid LINE signature")

    data = json.loads(body.decode("utf-8"))
    events = data.get("events", [])
    for ev in events:
        if ev.get("type") != "message":
            continue
        msg = ev.get("message", {})
        if msg.get("type") != "text":
            continue

        text = str(msg.get("text", "")).strip().lower()
        reply_token = ev.get("replyToken", "")

        if text == "help":
            reply_line_text(
                reply_token,
                "cmd: status | lock door | unlock door | lock window | unlock window | lock all | unlock all",
            )
            continue

        if not is_supported_cmd(text):
            reply_line_text(reply_token, "unsupported cmd, send 'help'")
            continue

        publish_cmd(text)
        reply_line_text(reply_token, f"sent: {text}")

    return {"ok": True}


def main() -> None:
    thread = threading.Thread(target=mqtt_loop_thread, daemon=True)
    thread.start()

    import uvicorn

    uvicorn.run(app, host=HTTP_HOST, port=HTTP_PORT, log_level="info")


if __name__ == "__main__":
    main()
