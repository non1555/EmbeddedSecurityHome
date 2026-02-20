import base64
import hashlib
import hmac
import json
import os
import threading
import time
from pathlib import Path
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
ROOT = Path(__file__).resolve().parent

MQTT_BROKER = env("MQTT_BROKER", "127.0.0.1")
MQTT_PORT = int(env("MQTT_PORT", "1883"))
MQTT_USERNAME = env("MQTT_USERNAME")
MQTT_PASSWORD = env("MQTT_PASSWORD")
MQTT_CLIENT_ID = env("MQTT_CLIENT_ID", "esh-line-bridge")
MQTT_TOPIC_CMD = env("MQTT_TOPIC_CMD", "esh/main/cmd")
MQTT_TOPIC_EVENT = env("MQTT_TOPIC_EVENT", "esh/main/event")
MQTT_TOPIC_STATUS = env("MQTT_TOPIC_STATUS", "esh/main/status")
MQTT_TOPIC_ACK = env("MQTT_TOPIC_ACK", "esh/main/ack")
MQTT_TOPIC_METRICS = env("MQTT_TOPIC_METRICS", "esh/main/metrics")
METRICS_PUSH_PERIOD_S = max(5, int(env("METRICS_PUSH_PERIOD_S", "30")))
INTRUDER_NOTIFY_COOLDOWN_S = max(0, int(env("INTRUDER_NOTIFY_COOLDOWN_S", "20")))
BRIDGE_CMD_TOKEN = env("BRIDGE_CMD_TOKEN", env("FW_CMD_TOKEN", "")).strip()
NONCE_STATE_FILE = Path(env("BRIDGE_NONCE_STATE_FILE", str(ROOT / ".nonce_state")))

LINE_CHANNEL_ACCESS_TOKEN = env("LINE_CHANNEL_ACCESS_TOKEN")
LINE_CHANNEL_SECRET = env("LINE_CHANNEL_SECRET")
LINE_TARGET_USER_ID = env("LINE_TARGET_USER_ID")
LINE_TARGET_GROUP_ID = env("LINE_TARGET_GROUP_ID")
LINE_TARGET_ROOM_ID = env("LINE_TARGET_ROOM_ID")

CMD_DEBOUNCE_MS = max(0, int(env("CMD_DEBOUNCE_MS", "600")))

HTTP_HOST = env("HTTP_HOST", "0.0.0.0")
HTTP_PORT = int(env("HTTP_PORT", "8080"))

LOCK_COMMANDS = {
    "lock door",
    "unlock door",
    "lock window",
    "unlock window",
    "lock all",
    "unlock all",
}
READ_ONLY_COMMANDS = {"status"}
SUPPORTED_COMMANDS = LOCK_COMMANDS | READ_ONLY_COMMANDS

INTRUDER_LEVELS = {"alert"}
INTRUDER_EVENT_TRIGGERS = {
    "door_open",
    "window_open",
    "door_tamper",
    "vib_spike",
    "motion",
    "chokepoint",
    "entry_timeout",
}

MODE_LABELS = {
    "startup_safe": "Startup Safe",
    "disarm": "Disarmed",
    "away": "Away Guard",
}

LEVEL_LABELS = {
    "off": "Normal",
    "warn": "Warning",
    "alert": "Alert",
}

EVENT_LABELS = {
    "door_open": "Door opened",
    "window_open": "Window opened",
    "door_tamper": "Door tamper detected",
    "vib_spike": "Vibration spike detected",
    "motion": "Motion detected",
    "chokepoint": "Chokepoint movement detected",
    "entry_timeout": "Entry delay timeout",
    "keypad_help_request": "Help requested from keypad",
    "arm_away": "Switched to away guard",
    "disarm": "System disarmed",
}

COMMAND_LABELS = {
    "lock door": "Lock door",
    "unlock door": "Unlock door",
    "lock window": "Lock window",
    "unlock window": "Unlock window",
    "lock all": "Lock all",
    "unlock all": "Unlock all",
    "status": "Status check",
}


def get_line_target() -> Optional[str]:
    if LINE_TARGET_USER_ID:
        return LINE_TARGET_USER_ID
    if LINE_TARGET_GROUP_ID:
        return LINE_TARGET_GROUP_ID
    if LINE_TARGET_ROOM_ID:
        return LINE_TARGET_ROOM_ID
    if state.auto_line_target and state.auto_line_target[0] in {"U", "C", "R"}:
        return state.auto_line_target
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
        event = _norm_text(obj.get("event", ""))
        cmd = _norm_text(obj.get("cmd", ""))
        return (
            "System Event\n"
            f"- Event: {_label_event(event)}\n"
            f"- Mode: {_label_mode(obj.get('mode', ''))}\n"
            f"- Risk: {_label_level(obj.get('level', ''))}\n"
            f"- Action: {_label_command(cmd)}"
        )
    if topic == MQTT_TOPIC_STATUS:
        return (
            "System Status\n"
            f"- Reason: {_label_event(obj.get('reason', ''))}\n"
            f"- Mode: {_label_mode(obj.get('mode', ''))}\n"
            f"- Risk: {_label_level(obj.get('level', ''))}\n"
            f"- Uptime: {_format_uptime_ms(obj.get('uptime_ms', '-'))}"
        )
    if topic == MQTT_TOPIC_ACK:
        cmd = _norm_text(obj.get("cmd", ""))
        ok = _json_bool(obj, "ok")
        result = "Success" if ok is True else ("Failed" if ok is False else "-")
        return (
            "Command Result\n"
            f"- Command: {_label_command(cmd)}\n"
            f"- Result: {result}\n"
            f"- Detail: {obj.get('detail', '-')}"
        )
    return (
        "Bridge Message\n"
        f"- Topic: {topic}\n"
        f"- Payload: {payload}"
    )


def _norm_text(v: Any) -> str:
    return str(v or "").strip().lower()


def _label_mode(v: Any) -> str:
    key = _norm_text(v)
    return MODE_LABELS.get(key, key or "-")


def _label_level(v: Any) -> str:
    key = _norm_text(v)
    return LEVEL_LABELS.get(key, key or "-")


def _label_event(v: Any) -> str:
    key = _norm_text(v)
    return EVENT_LABELS.get(key, key or "-")


def _label_command(v: Any) -> str:
    key = _norm_text(v)
    return COMMAND_LABELS.get(key, key or "-")


def _format_uptime_ms(v: Any) -> str:
    try:
        ms = int(v)
        if ms < 0:
            return "-"
        sec = ms // 1000
        h = sec // 3600
        m = (sec % 3600) // 60
        s = sec % 60
        return f"{h:02d}:{m:02d}:{s:02d}"
    except Exception:
        return "-"


def _is_intruder_signal(topic: str, obj: Dict[str, Any]) -> bool:
    level = _norm_text(obj.get("level", ""))
    if level not in INTRUDER_LEVELS:
        return False

    if topic == MQTT_TOPIC_EVENT:
        return _norm_text(obj.get("event", "")) in INTRUDER_EVENT_TRIGGERS
    if topic == MQTT_TOPIC_STATUS:
        return _norm_text(obj.get("reason", "")) in INTRUDER_EVENT_TRIGGERS
    return False


def _is_keypad_help_signal(topic: str, obj: Dict[str, Any]) -> bool:
    if topic != MQTT_TOPIC_EVENT:
        return False
    return _norm_text(obj.get("event", "")) == "keypad_help_request"


def _format_keypad_help_alert(obj: Dict[str, Any]) -> str:
    return (
        "Help Request\n"
        "A keypad help request was triggered.\n"
        f"- Mode: {_label_mode(obj.get('mode', ''))}\n"
        f"- Risk: {_label_level(obj.get('level', ''))}"
    )


def _format_intruder_alert(topic: str, obj: Dict[str, Any]) -> str:
    trigger = _norm_text(obj.get("event", "")) if topic == MQTT_TOPIC_EVENT else _norm_text(obj.get("reason", ""))
    return (
        "Security Alert\n"
        "Possible intrusion detected.\n"
        f"- Trigger: {_label_event(trigger)}\n"
        f"- Mode: {_label_mode(obj.get('mode', ''))}\n"
        f"- Risk: {_label_level(obj.get('level', ''))}"
    )


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
    # Backward-compatible quick reply menu (kept as fallback).
    items = [
        {"type": "action", "action": {"type": "postback", "label": "Home", "data": "ui=home"}},
        {"type": "action", "action": {"type": "postback", "label": "Lock Door", "data": "cmd=lock door"}},
        {"type": "action", "action": {"type": "postback", "label": "Unlock Door", "data": "cmd=unlock door"}},
        {"type": "action", "action": {"type": "postback", "label": "Lock Window", "data": "cmd=lock window"}},
        {"type": "action", "action": {"type": "postback", "label": "Unlock Window", "data": "cmd=unlock window"}},
        {"type": "action", "action": {"type": "postback", "label": "Lock All", "data": "cmd=lock all"}},
        {"type": "action", "action": {"type": "postback", "label": "Unlock All", "data": "cmd=unlock all"}},
        {"type": "action", "action": {"type": "postback", "label": "Status", "data": "cmd=status"}},
    ]
    mode = state.dev_mode or state.last_status_mode or "unknown"
    return {"type": "text", "text": f"Menu (mode={mode})", "quickReply": {"items": items}}


def parse_postback_cmd(data: str) -> str:
    s = (data or "").strip()
    if s.startswith("cmd="):
        return s[4:].strip().lower()
    if s.startswith("cmd:"):
        return s[4:].strip().lower()
    return s.strip().lower()


def _fmt_bool(b: Optional[bool], t: str, f: str, u: str = "?") -> str:
    if b is True:
        return t
    if b is False:
        return f
    return u


def _device_summary_line() -> str:
    mode = _label_mode(state.dev_mode or state.last_status_mode or "unknown")
    dl = _fmt_bool(state.dev_door_locked, "LOCK", "UNLOCK")
    wl = _fmt_bool(state.dev_window_locked, "LOCK", "UNLOCK")
    do = _fmt_bool(state.dev_door_open, "OPEN", "CLOSE")
    wo = _fmt_bool(state.dev_window_open, "OPEN", "CLOSE")
    return f"Mode: {mode} | Door: {dl}/{do} | Window: {wl}/{wo}"


def _action(label: str, data: str) -> Dict[str, Any]:
    return {"type": "button", "action": {"type": "postback", "label": label[:20], "data": data}}


def flex_home() -> Dict[str, Any]:
    # Rich UI inside chat (Flex) without uploading images.
    return {
        "type": "flex",
        "altText": "EmbeddedSecurity menu",
        "contents": {
            "type": "bubble",
            "size": "kilo",
            "body": {
                "type": "box",
                "layout": "vertical",
                "spacing": "md",
                "contents": [
                    {"type": "text", "text": "EmbeddedSecurity Home", "weight": "bold", "size": "lg"},
                    {"type": "text", "text": _device_summary_line(), "size": "sm", "wrap": True, "color": "#666666"},
                    {"type": "separator"},
                    {
                        "type": "box",
                        "layout": "vertical",
                        "spacing": "md",
                        "contents": [
                            {
                                "type": "button",
                                "style": "secondary",
                                "action": {"type": "postback", "label": "Notifications", "data": "ui=mode"},
                            },
                            {
                                "type": "button",
                                "style": "primary",
                                "action": {"type": "postback", "label": "Door & Window", "data": "ui=lock"},
                            },
                            {
                                "type": "button",
                                "style": "link",
                                "action": {"type": "postback", "label": "Check Status", "data": "cmd=status"},
                            },
                        ],
                    },
                ],
            },
        },
    }


def flex_mode() -> Dict[str, Any]:
    # Kept for backward compatibility with old rich menu mapping (ui=mode).
    return {
        "type": "flex",
        "altText": "Notification view",
        "contents": {
            "type": "bubble",
            "size": "kilo",
            "body": {
                "type": "box",
                "layout": "vertical",
                "spacing": "md",
                "contents": [
                    {"type": "text", "text": "Notifications", "weight": "bold", "size": "lg"},
                    {"type": "text", "text": _device_summary_line(), "size": "sm", "wrap": True, "color": "#666666"},
                    {"type": "separator"},
                    {"type": "text", "text": "This chat will show alerts, status updates, and command results.", "size": "sm", "wrap": True},
                    {"type": "text", "text": "Supported commands: lock/unlock/status", "size": "sm", "wrap": True, "color": "#666666"},
                    {"type": "button", "style": "link", "action": {"type": "postback", "label": "Back", "data": "ui=home"}},
                ],
            },
        },
    }


def flex_lock() -> Dict[str, Any]:
    # Button label should represent the action (opposite of current state).
    if state.dev_door_locked is True:
        door_label = "Unlock Door"
        door_cmd = "cmd=unlock door"
    elif state.dev_door_locked is False:
        door_label = "Lock Door"
        door_cmd = "cmd=lock door"
    else:
        door_label = "Lock Door"
        door_cmd = "cmd=lock door"

    if state.dev_window_locked is True:
        win_label = "Unlock Window"
        win_cmd = "cmd=unlock window"
    elif state.dev_window_locked is False:
        win_label = "Lock Window"
        win_cmd = "cmd=lock window"
    else:
        win_label = "Lock Window"
        win_cmd = "cmd=lock window"

    return {
        "type": "flex",
        "altText": "Door and window controls",
        "contents": {
            "type": "bubble",
            "size": "kilo",
            "body": {
                "type": "box",
                "layout": "vertical",
                "spacing": "md",
                "contents": [
                    {"type": "text", "text": "Door and Window", "weight": "bold", "size": "lg"},
                    {"type": "text", "text": _device_summary_line(), "size": "sm", "wrap": True, "color": "#666666"},
                    {"type": "separator"},
                    {"type": "button", "style": "primary", "action": {"type": "postback", "label": door_label, "data": door_cmd}},
                    {"type": "button", "style": "primary", "action": {"type": "postback", "label": win_label, "data": win_cmd}},
                    {"type": "button", "style": "secondary", "action": {"type": "postback", "label": "Lock All", "data": "cmd=lock all"}},
                    {"type": "button", "style": "secondary", "action": {"type": "postback", "label": "Unlock All", "data": "cmd=unlock all"}},
                    {"type": "button", "style": "link", "action": {"type": "postback", "label": "Back", "data": "ui=home"}},
                ],
            },
        },
    }


class BridgeState:
    def __init__(self) -> None:
        self.mqtt_connected = False
        self.last_mqtt_rx_topic = ""
        self.last_mqtt_rx_payload = ""
        self.last_mqtt_rx_at = 0.0
        self.last_cmd = ""
        self.last_cmd_at = 0.0
        self.last_metrics_push_at = 0.0
        self.last_intruder_push_at = 0.0
        self.last_status_mode = ""
        self.last_status_level = ""
        self.last_status_at = 0.0
        # Device snapshot (best-effort, populated from MQTT event/status/ack).
        self.dev_mode = ""
        self.dev_level = ""
        self.dev_door_locked: Optional[bool] = None
        self.dev_window_locked: Optional[bool] = None
        self.dev_door_open: Optional[bool] = None
        self.dev_window_open: Optional[bool] = None
        self.dev_at = 0.0
        # Learned from LINE webhook when LINE_TARGET_* isn't configured.
        self.auto_line_target = ""


state = BridgeState()
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=MQTT_CLIENT_ID, clean_session=True)

_last_cmd_at_by_source_ms: Dict[str, int] = {}


def _load_nonce_state() -> int:
    try:
        raw = NONCE_STATE_FILE.read_text(encoding="utf-8", errors="replace").strip()
        v = int(raw or "0")
        return v if v > 0 else 0
    except Exception:
        return 0


def _save_nonce_state(value: int) -> None:
    try:
        NONCE_STATE_FILE.write_text(str(int(value)), encoding="utf-8")
    except Exception:
        pass


_last_cmd_nonce: int = _load_nonce_state()


def _next_cmd_nonce() -> int:
    global _last_cmd_nonce
    candidate = int(time.time()) & 0xFFFFFFFF
    if candidate <= 0:
        candidate = 1
    if candidate <= _last_cmd_nonce:
        candidate = (_last_cmd_nonce + 1) & 0xFFFFFFFF
        if candidate == 0:
            candidate = 1
    _last_cmd_nonce = candidate
    _save_nonce_state(_last_cmd_nonce)
    return candidate


def _encode_command_payload(cmd: str) -> str:
    text = cmd.strip().lower()
    if not BRIDGE_CMD_TOKEN:
        return text
    nonce = _next_cmd_nonce()
    return f"{BRIDGE_CMD_TOKEN}|{nonce}|{text}"


def command_auth_ready() -> bool:
    return bool(BRIDGE_CMD_TOKEN)


def is_read_only_cmd(text: str) -> bool:
    return text in READ_ONLY_COMMANDS


def source_key(ev: Dict[str, Any]) -> str:
    src = ev.get("source", {}) if isinstance(ev.get("source", {}), dict) else {}
    return (
        # Prefer group/room so pushes go back to the chat.
        str(src.get("groupId") or "")
        or str(src.get("roomId") or "")
        or str(src.get("userId") or "")
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


def mqtt_publish_ok(topic: str, payload: str, qos: int = 0, retain: bool = False) -> bool:
    try:
        info = mqtt_client.publish(topic, payload=payload, qos=qos, retain=retain)
    except Exception:
        return False

    rc = getattr(info, "rc", None)
    if rc is None and isinstance(info, tuple) and info:
        rc = info[0]
    if rc is None:
        # Best-effort fallback for unknown client adapters.
        return True
    return int(rc) == int(mqtt.MQTT_ERR_SUCCESS)


def publish_cmd(cmd: str) -> bool:
    text = cmd.strip().lower()
    if not text or not is_supported_cmd(text):
        return False
    if not command_auth_ready() and not is_read_only_cmd(text):
        return False
    payload = _encode_command_payload(text) if command_auth_ready() else text
    if not mqtt_publish_ok(MQTT_TOPIC_CMD, payload=payload, qos=0, retain=False):
        return False
    state.last_cmd = text
    state.last_cmd_at = time.time()
    return True


def is_supported_cmd(text: str) -> bool:
    return text in SUPPORTED_COMMANDS


def _json_bool(obj: Dict[str, Any], key: str) -> Optional[bool]:
    if key not in obj:
        return None
    v = obj.get(key)
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return bool(v)
    if isinstance(v, str):
        s = v.strip().lower()
        if s in {"true", "1", "yes", "on"}:
            return True
        if s in {"false", "0", "no", "off"}:
            return False
    return None


def _apply_snapshot_from_obj(obj: Dict[str, Any]) -> None:
    updated = False
    mode = str(obj.get("mode", "") or "")
    level = str(obj.get("level", "") or "")
    if mode:
        state.dev_mode = mode
        updated = True
    if level:
        state.dev_level = level
        updated = True

    dl = _json_bool(obj, "door_locked")
    wl = _json_bool(obj, "window_locked")
    do = _json_bool(obj, "door_open")
    wo = _json_bool(obj, "window_open")
    if dl is not None:
        state.dev_door_locked = dl
        updated = True
    if wl is not None:
        state.dev_window_locked = wl
        updated = True
    if do is not None:
        state.dev_door_open = do
        updated = True
    if wo is not None:
        state.dev_window_open = wo
        updated = True

    if updated:
        state.dev_at = time.time()


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
    push_line_text("Bridge connected to MQTT. LINE notifications are active.")


def on_disconnect(client: mqtt.Client, userdata: Any, disconnect_flags: Any, reason_code: Any, properties: Any) -> None:
    state.mqtt_connected = False
    push_line_text(f"Bridge disconnected from MQTT (rc={reason_code}). Trying to reconnect.")


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
        _apply_snapshot_from_obj(obj)
    if topic == MQTT_TOPIC_EVENT:
        obj = parse_json_payload(payload)
        _apply_snapshot_from_obj(obj)
    if topic == MQTT_TOPIC_ACK:
        obj = parse_json_payload(payload)
        detail = str(obj.get("detail", "") or "")
        # Compact firmware detail format: dL=1,wL=0,dO=0,wO=1
        kv: Dict[str, str] = {}
        for part in detail.split(","):
            part = part.strip()
            if "=" not in part:
                continue
            k, v = part.split("=", 1)
            kv[k.strip()] = v.strip()

        def b(name: str) -> Optional[bool]:
            if name not in kv:
                return None
            return kv[name] in {"1", "true", "True", "yes", "Y", "on"}

        dl = b("dL")
        wl = b("wL")
        do = b("dO")
        wo = b("wO")
        if dl is not None:
            state.dev_door_locked = dl
        if wl is not None:
            state.dev_window_locked = wl
        if do is not None:
            state.dev_door_open = do
        if wo is not None:
            state.dev_window_open = wo
        if any(x is not None for x in (dl, wl, do, wo)):
            state.dev_at = time.time()
    if topic == MQTT_TOPIC_METRICS:
      now = time.time()
      if (now - state.last_metrics_push_at) < METRICS_PUSH_PERIOD_S:
          return
      state.last_metrics_push_at = now
      obj = parse_json_payload(payload)
      push_line_text(
          "System Health\n"
          f"- Queue us/pub/cmd/store: {obj.get('q_us', '-')}/{obj.get('q_pub', '-')}/{obj.get('q_cmd', '-')}/{obj.get('q_store', '-')}\n"
          f"- Drops us/pub/cmd/store: {obj.get('us_drops', '-')}/{obj.get('pub_drops', '-')}/{obj.get('cmd_drops', '-')}/{obj.get('store_drops', '-')}"
      )
      return
    if topic == MQTT_TOPIC_EVENT or topic == MQTT_TOPIC_STATUS:
        obj = parse_json_payload(payload)
        if _is_keypad_help_signal(topic, obj):
            push_line_text(_format_keypad_help_alert(obj))
            return
        if _is_intruder_signal(topic, obj):
            now = time.time()
            if INTRUDER_NOTIFY_COOLDOWN_S == 0 or (now - state.last_intruder_push_at) >= INTRUDER_NOTIFY_COOLDOWN_S:
                state.last_intruder_push_at = now
                push_line_text(_format_intruder_alert(topic, obj))
            return
    # Avoid spamming LINE with UI-driven polling.
    if topic == MQTT_TOPIC_ACK:
        obj = parse_json_payload(payload)
        if str(obj.get("cmd", "") or "") == "status":
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

@app.get("/state")
def get_state() -> Dict[str, Any]:
    return {
        "ok": True,
        "at": time.time(),
        "device": {
            "mode": state.dev_mode or state.last_status_mode or "",
            "level": state.dev_level or state.last_status_level or "",
            "door_locked": state.dev_door_locked,
            "window_locked": state.dev_window_locked,
            "door_open": state.dev_door_open,
            "window_open": state.dev_window_open,
            "updated_at": state.dev_at,
        },
    }


@app.post("/cmd")
async def http_cmd(request: Request) -> Dict[str, Any]:
    data = await request.json()
    cmd = str((data or {}).get("cmd", "")).strip().lower()
    if not cmd or not is_supported_cmd(cmd):
        raise HTTPException(status_code=400, detail="unsupported cmd")
    if not command_auth_ready() and not is_read_only_cmd(cmd):
        raise HTTPException(status_code=503, detail="command auth token missing")
    if not publish_cmd(cmd):
        raise HTTPException(status_code=503, detail="command publish blocked")
    return {"ok": True, "cmd": cmd}


@app.get("/health")
def health() -> Dict[str, Any]:
    target = get_line_target()
    problems = []

    if not state.mqtt_connected:
        problems.append("mqtt_disconnected")

    # LINE push requires: token + a push target. Webhook verify requires: channel secret.
    line_token_ok = bool(LINE_CHANNEL_ACCESS_TOKEN)
    line_secret_ok = bool(LINE_CHANNEL_SECRET)
    line_target_ok = bool(target)
    line_push_ready = line_token_ok and line_target_ok
    line_webhook_ready = line_token_ok and line_secret_ok

    if not line_token_ok:
        problems.append("line_access_token_missing")
    if not line_secret_ok:
        problems.append("line_channel_secret_missing")
    if not line_target_ok:
        problems.append("line_target_missing")
    if not command_auth_ready():
        problems.append("cmd_auth_token_missing")

    ready = bool(state.mqtt_connected and line_push_ready and line_webhook_ready)
    return {
        "ok": True,
        "ready": ready,
        "problems": problems,
        "mqtt_connected": state.mqtt_connected,
        "mqtt_broker": MQTT_BROKER,
        "mqtt_port": MQTT_PORT,
        "last_mqtt_topic": state.last_mqtt_rx_topic,
        "last_cmd": state.last_cmd,
        "line_webhook_ready": line_webhook_ready,
        "line_push_ready": line_push_ready,
        "line_target_configured": bool(target),
        "cmd_auth_token_configured": bool(BRIDGE_CMD_TOKEN),
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

        if not (LINE_TARGET_USER_ID or LINE_TARGET_GROUP_ID or LINE_TARGET_ROOM_ID):
            if src_k and src_k != "unknown":
                state.auto_line_target = src_k

        if ev.get("type") == "postback":
            data_pb = str(ev.get("postback", {}).get("data", "") or "").strip()
            if not data_pb:
                continue

            # UI navigation within chat (Flex)
            if data_pb.startswith("ui="):
                page = data_pb[3:].strip().lower()
                if page == "mode":
                    reply_line_messages(reply_token, [flex_mode()])
                elif page == "lock":
                    reply_line_messages(reply_token, [flex_lock()])
                else:
                    reply_line_messages(reply_token, [flex_home()])
                continue

            cmd = parse_postback_cmd(data_pb)
            if not cmd:
                continue
            if cmd in {"help", "menu"}:
                reply_line_messages(reply_token, [flex_home()])
                continue
            if not is_supported_cmd(cmd):
                reply_line_text(reply_token, "Command not supported. Send 'menu' to use quick actions.")
                continue
            if not command_auth_ready() and not is_read_only_cmd(cmd):
                reply_line_text(reply_token, "Command is currently disabled. Admin needs to set command token.")
                continue
            if not debounce_ok(src_k, cmd, ev_ts_ms):
                # Silent debounce: no chat spam.
                continue
            if not publish_cmd(cmd):
                reply_line_text(reply_token, "Could not send command right now. Please try again.")
                continue
            # Keep chat clean: show menu again instead of "sent: ..."
            reply_line_messages(reply_token, [flex_home()])
            continue

        if ev.get("type") != "message":
            continue
        msg = ev.get("message", {})
        if msg.get("type") != "text":
            continue

        text = str(msg.get("text", "")).strip().lower()

        if text in {"help", "menu"}:
            reply_line_messages(reply_token, [flex_home()])
            continue

        if not is_supported_cmd(text):
            reply_line_text(reply_token, "Command not supported. Send 'menu' to use quick actions.")
            continue
        if not command_auth_ready() and not is_read_only_cmd(text):
            reply_line_text(reply_token, "Command is currently disabled. Admin needs to set command token.")
            continue

        if not debounce_ok(src_k, text, ev_ts_ms):
            reply_line_text(reply_token, "You sent this too quickly. Please wait a moment and try again.")
            continue

        if not publish_cmd(text):
            reply_line_text(reply_token, "Could not send command right now. Please try again.")
            continue
        reply_line_messages(reply_token, [flex_home()])

    return {"ok": True}


def main() -> None:
    thread = threading.Thread(target=mqtt_loop_thread, daemon=True)
    thread.start()

    import uvicorn

    uvicorn.run(app, host=HTTP_HOST, port=HTTP_PORT, log_level="info")


if __name__ == "__main__":
    main()
