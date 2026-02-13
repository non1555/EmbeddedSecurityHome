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

# Rich menu IDs per mode (images are static; we switch rich menu per user).
LINE_RICHMENU_ID_DISARM = env("LINE_RICHMENU_ID_DISARM")
LINE_RICHMENU_ID_NIGHT = env("LINE_RICHMENU_ID_NIGHT")
LINE_RICHMENU_ID_AWAY = env("LINE_RICHMENU_ID_AWAY")

CMD_DEBOUNCE_MS = max(0, int(env("CMD_DEBOUNCE_MS", "600")))

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
    reply_line_messages(reply_token, [{"type": "text", "text": text[:4800]}])


def reply_line_messages(reply_token: str, messages: Any) -> None:
    if not LINE_CHANNEL_ACCESS_TOKEN:
        return
    body = {
        "replyToken": reply_token,
        "messages": messages,
    }
    requests.post(
        "https://api.line.me/v2/bot/message/reply",
        headers=line_api_headers(),
        json=body,
        timeout=8,
    )


def menu_message() -> Dict[str, Any]:
    # Use Quick Reply so user can tap buttons instead of typing.
    items = [
        # No displayText: keep chat clean (tap does not echo a user message).
        {"type": "action", "action": {"type": "postback", "label": "Status", "data": "cmd=status"}},
        {"type": "action", "action": {"type": "postback", "label": "Disarm", "data": "cmd=disarm"}},
        {"type": "action", "action": {"type": "postback", "label": "Arm Night", "data": "cmd=arm night"}},
        {"type": "action", "action": {"type": "postback", "label": "Arm Away", "data": "cmd=arm away"}},
        {"type": "action", "action": {"type": "postback", "label": "Lock Door", "data": "cmd=lock door"}},
        {"type": "action", "action": {"type": "postback", "label": "Unlock Door", "data": "cmd=unlock door"}},
        {"type": "action", "action": {"type": "postback", "label": "Lock All", "data": "cmd=lock all"}},
        {"type": "action", "action": {"type": "postback", "label": "Unlock All", "data": "cmd=unlock all"}},
        {"type": "action", "action": {"type": "postback", "label": "Alarm", "data": "cmd=alarm"}},
        {"type": "action", "action": {"type": "postback", "label": "Silence", "data": "cmd=silence"}},
    ]
    mode = state.last_status_mode or "unknown"
    return {
        "type": "text",
        "text": f"Select command (mode={mode}):",
        "quickReply": {"items": items},
    }


def parse_postback_cmd(data: str) -> str:
    s = (data or "").strip()
    if s.startswith("cmd="):
        return s[4:].strip().lower()
    if s.startswith("cmd:"):
        return s[4:].strip().lower()
    return s.strip().lower()


class BridgeState:
    def __init__(self) -> None:
        self.mqtt_connected = False
        self.last_mqtt_rx_topic = ""
        self.last_mqtt_rx_payload = ""
        self.last_mqtt_rx_at = 0.0
        self.last_cmd = ""
        self.last_cmd_at = 0.0
        self.last_metrics_push_at = 0.0
        self.last_status_mode = ""
        self.last_status_level = ""
        self.last_status_at = 0.0


state = BridgeState()
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=MQTT_CLIENT_ID, clean_session=True)

_last_cmd_at_by_source_ms: Dict[str, int] = {}


def source_key(ev: Dict[str, Any]) -> str:
    src = ev.get("source", {}) if isinstance(ev.get("source", {}), dict) else {}
    return (
        str(src.get("userId") or "")
        or str(src.get("groupId") or "")
        or str(src.get("roomId") or "")
        or "unknown"
    )


def debounce_ok(src_key: str, cmd: str, ev_ts_ms: int) -> bool:
    if CMD_DEBOUNCE_MS <= 0:
        return True
    wall_ms = int(time.time() * 1000)
    # Prefer LINE event timestamp, but guard against missing/garbage/very old values.
    ts_ms = int(ev_ts_ms or 0)
    if ts_ms <= 0:
        now_ms = wall_ms
    else:
        # Accept timestamps within 1 day behind or 5 minutes ahead.
        if (wall_ms - ts_ms) > 86_400_000 or (ts_ms - wall_ms) > 300_000:
            now_ms = wall_ms
        else:
            now_ms = ts_ms
    k = f"{src_key}|{cmd}"
    last_ms = _last_cmd_at_by_source_ms.get(k, 0)
    if now_ms - last_ms < CMD_DEBOUNCE_MS:
        return False
    _last_cmd_at_by_source_ms[k] = now_ms
    # Prevent unbounded growth (cheap pruning).
    if len(_last_cmd_at_by_source_ms) > 500:
        cutoff_ms = now_ms - 30_000
        for kk, vv in list(_last_cmd_at_by_source_ms.items()):
            if vv < cutoff_ms:
                _last_cmd_at_by_source_ms.pop(kk, None)
    return True


def richmenu_id_for_mode(mode: str) -> str:
    m = (mode or "").strip().lower()
    if m == "disarm":
        return LINE_RICHMENU_ID_DISARM
    if m == "night":
        return LINE_RICHMENU_ID_NIGHT
    if m == "away":
        return LINE_RICHMENU_ID_AWAY
    return ""


def link_richmenu_to_user(user_id: str, richmenu_id: str) -> None:
    if not LINE_CHANNEL_ACCESS_TOKEN or not user_id or not richmenu_id:
        return
    url = f"https://api.line.me/v2/bot/user/{user_id}/richmenu/{richmenu_id}"
    try:
        r = requests.post(url, headers=line_api_headers(), timeout=8)
        # Ignore failures (userId may be missing/invalid, bot may not be 1:1 chat, etc.)
        if r.status_code // 100 != 2:
            return
    except Exception:
        return


def maybe_update_user_richmenu(ev: Dict[str, Any]) -> None:
    src = ev.get("source", {}) if isinstance(ev.get("source", {}), dict) else {}
    user_id = str(src.get("userId") or "")
    if not user_id:
        return
    rm = richmenu_id_for_mode(state.last_status_mode)
    if not rm:
        return
    link_richmenu_to_user(user_id, rm)


def maybe_update_user_richmenu_for_cmd(ev: Dict[str, Any], cmd: str) -> None:
    """
    Rich menu images are static; when user taps a mode command, switch their rich menu
    optimistically to the target mode so UI matches immediately.
    """
    implied_mode = ""
    c = (cmd or "").strip().lower()
    if c in {"disarm", "mode disarm"}:
        implied_mode = "disarm"
    elif c in {"arm night", "arm_night", "mode night"}:
        implied_mode = "night"
    elif c in {"arm away", "arm_away", "mode away"}:
        implied_mode = "away"

    src = ev.get("source", {}) if isinstance(ev.get("source", {}), dict) else {}
    user_id = str(src.get("userId") or "")
    if not user_id:
        return

    rm = richmenu_id_for_mode(implied_mode) if implied_mode else richmenu_id_for_mode(state.last_status_mode)
    if not rm:
        return
    link_richmenu_to_user(user_id, rm)


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
        "disarm",
        "arm night",
        "arm away",
        "buzz",
        "buzz warn",
        "buzz alarm",
        "alarm",
        "alarm on",
        "alarm off",
        "silence",
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
    if topic == MQTT_TOPIC_STATUS:
        obj = parse_json_payload(payload)
        state.last_status_mode = str(obj.get("mode", "") or "")
        state.last_status_level = str(obj.get("level", "") or "")
        state.last_status_at = time.time()
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
        reply_token = ev.get("replyToken", "")
        src_k = source_key(ev)
        ev_ts_ms = int(ev.get("timestamp") or 0)

        if ev.get("type") == "postback":
            cmd = parse_postback_cmd(str(ev.get("postback", {}).get("data", "")))
            if not cmd:
                continue
            if cmd in {"help", "menu"}:
                maybe_update_user_richmenu(ev)
                reply_line_messages(reply_token, [menu_message()])
                continue
            if not is_supported_cmd(cmd):
                reply_line_text(reply_token, "unsupported cmd, send 'menu'")
                continue
            if not debounce_ok(src_k, cmd, ev_ts_ms):
                # Silent debounce: no chat spam.
                continue
            maybe_update_user_richmenu_for_cmd(ev, cmd)
            publish_cmd(cmd)
            # Silent success for UI taps: no chat spam.
            continue

        if ev.get("type") != "message":
            continue
        msg = ev.get("message", {})
        if msg.get("type") != "text":
            continue

        text = str(msg.get("text", "")).strip().lower()

        if text in {"help", "menu"}:
            maybe_update_user_richmenu(ev)
            reply_line_messages(reply_token, [menu_message()])
            continue

        if not is_supported_cmd(text):
            reply_line_text(reply_token, "unsupported cmd, send 'menu'")
            continue

        if not debounce_ok(src_k, text, ev_ts_ms):
            reply_line_text(reply_token, "ignored (debounce), try again")
            continue

        maybe_update_user_richmenu_for_cmd(ev, text)
        publish_cmd(text)
        reply_line_text(reply_token, f"sent: {text} (mode={state.last_status_mode or 'unknown'})")

    return {"ok": True}


def main() -> None:
    thread = threading.Thread(target=mqtt_loop_thread, daemon=True)
    thread.start()

    import uvicorn

    uvicorn.run(app, host=HTTP_HOST, port=HTTP_PORT, log_level="info")


if __name__ == "__main__":
    main()
