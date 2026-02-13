import base64
import hashlib
import hmac
import json
import os
import time

import paho.mqtt.client as mqtt
import requests


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


def sign_line(body: bytes, secret: str) -> str:
    digest = hmac.new(secret.encode("utf-8"), body, hashlib.sha256).digest()
    return base64.b64encode(digest).decode("utf-8")


def main() -> int:
    load_env_file(".env")

    mqtt_broker = env("MQTT_BROKER", "127.0.0.1")
    mqtt_port = int(env("MQTT_PORT", "1883"))
    mqtt_user = env("MQTT_USERNAME")
    mqtt_pass = env("MQTT_PASSWORD")

    topic_cmd = env("MQTT_TOPIC_CMD", "esh/cmd")

    http_port = int(env("HTTP_PORT", "8080"))
    webhook_url = f"http://127.0.0.1:{http_port}/line/webhook"

    line_secret = env("LINE_CHANNEL_SECRET")

    got = {"cmd": []}

    def on_connect(client, userdata, flags, reason_code, properties):
        client.subscribe("esh/#", qos=0)

    def on_message(client, userdata, msg):
        try:
            payload = msg.payload.decode("utf-8", errors="replace")
        except Exception:
            payload = repr(msg.payload)
        print(f"[MQTT RX] {msg.topic} {payload}")
        if msg.topic == topic_cmd:
            got["cmd"].append(payload.strip())

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="esh-dev-test", clean_session=True)
    if mqtt_user:
        client.username_pw_set(mqtt_user, mqtt_pass)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(mqtt_broker, mqtt_port, keepalive=20)
    client.loop_start()

    time.sleep(0.4)

    # 1) Direct MQTT publish test (does broker accept publish/sub?).
    direct_cmd = "arm away"
    print(f"[TEST] publish direct MQTT cmd: {direct_cmd!r} -> {topic_cmd}")
    client.publish(topic_cmd, payload=direct_cmd, qos=0, retain=False)

    time.sleep(1.0)

    # 2) LINE webhook -> bridge -> MQTT publish test (if LINE secret available).
    if not line_secret:
        print("[SKIP] LINE_CHANNEL_SECRET missing in .env, skip webhook test")
    else:
        body_obj = {
            "events": [
                {
                    "type": "message",
                    "replyToken": "DUMMY_REPLY_TOKEN",
                    "message": {"type": "text", "text": "arm away"},
                }
            ]
        }
        raw = json.dumps(body_obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        sig = sign_line(raw, line_secret)
        print(f"[TEST] POST webhook -> {webhook_url}")
        r = requests.post(webhook_url, data=raw, headers={"X-Line-Signature": sig}, timeout=10)
        print(f"[HTTP] {r.status_code} {r.text[:200]}")

        # 3) Postback test (button UI path).
        body_obj = {
            "events": [
                {
                    "type": "postback",
                    "replyToken": "DUMMY_REPLY_TOKEN",
                    "source": {"userId": "Uxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"},
                    "timestamp": int(time.time() * 1000),
                    "postback": {"data": "cmd=alarm"},
                },
                {
                    # Debounce should ignore this second tap.
                    "type": "postback",
                    "replyToken": "DUMMY_REPLY_TOKEN",
                    "source": {"userId": "Uxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"},
                    "timestamp": int(time.time() * 1000),
                    "postback": {"data": "cmd=alarm"},
                },
            ]
        }
        raw = json.dumps(body_obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        sig = sign_line(raw, line_secret)
        print(f"[TEST] POST webhook postback -> {webhook_url}")
        r = requests.post(webhook_url, data=raw, headers={"X-Line-Signature": sig}, timeout=10)
        print(f"[HTTP] {r.status_code} {r.text[:200]}")

    time.sleep(2.0)
    client.loop_stop()
    client.disconnect()

    # Success criteria: at least one cmd observed on MQTT.
    if got["cmd"]:
        print(f"[OK] observed {len(got['cmd'])} cmd message(s) on {topic_cmd}")
        return 0

    print(f"[FAIL] did not observe any cmd messages on {topic_cmd}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
