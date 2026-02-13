import argparse
import json
import os
from typing import Any, Dict, List, Tuple

import requests
from PIL import Image, ImageDraw, ImageFont


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


def api_headers(access_token: str) -> Dict[str, str]:
    return {
        "Authorization": f"Bearer {access_token}",
        "Content-Type": "application/json",
    }


def api_post_json(url: str, access_token: str, payload: Dict[str, Any], timeout_s: int = 15) -> Dict[str, Any]:
    r = requests.post(url, headers=api_headers(access_token), json=payload, timeout=timeout_s)
    if r.status_code // 100 != 2:
        raise RuntimeError(f"POST {url} -> {r.status_code} {r.text[:400]}")
    try:
        return r.json()
    except Exception:
        return {}


def api_get_json(url: str, access_token: str, timeout_s: int = 15) -> Dict[str, Any]:
    r = requests.get(url, headers=api_headers(access_token), timeout=timeout_s)
    if r.status_code // 100 != 2:
        raise RuntimeError(f"GET {url} -> {r.status_code} {r.text[:400]}")
    try:
        return r.json()
    except Exception:
        return {}


def api_delete(url: str, access_token: str, timeout_s: int = 15) -> None:
    r = requests.delete(url, headers=api_headers(access_token), timeout=timeout_s)
    if r.status_code // 100 != 2:
        raise RuntimeError(f"DELETE {url} -> {r.status_code} {r.text[:400]}")


def upload_richmenu_image(richmenu_id: str, access_token: str, png_path: str, timeout_s: int = 30) -> None:
    url = f"https://api-data.line.me/v2/bot/richmenu/{richmenu_id}/content"
    with open(png_path, "rb") as f:
        data = f.read()
    headers = {
        "Authorization": f"Bearer {access_token}",
        "Content-Type": "image/png",
    }
    r = requests.post(url, headers=headers, data=data, timeout=timeout_s)
    if r.status_code // 100 != 2:
        raise RuntimeError(f"POST {url} -> {r.status_code} {r.text[:400]}")


def set_default_richmenu(richmenu_id: str, access_token: str) -> None:
    url = f"https://api.line.me/v2/bot/user/all/richmenu/{richmenu_id}"
    r = requests.post(url, headers=api_headers(access_token), timeout=15)
    if r.status_code // 100 != 2:
        raise RuntimeError(f"POST {url} -> {r.status_code} {r.text[:400]}")


def get_default_richmenu_id(access_token: str) -> str:
    url = "https://api.line.me/v2/bot/user/all/richmenu"
    r = requests.get(url, headers=api_headers(access_token), timeout=15)
    if r.status_code == 404:
        return ""
    if r.status_code // 100 != 2:
        raise RuntimeError(f"GET {url} -> {r.status_code} {r.text[:400]}")
    obj = r.json()
    return str(obj.get("richMenuId", "") or "")


def build_richmenu_2500x1686() -> Tuple[Dict[str, Any], List[Tuple[str, str]]]:
    """
    Returns (richmenu_payload, ordered_cmds) where ordered_cmds is used to keep
    the mapping in sync with the PNG mock.
    """
    w, h = 2500, 1686
    cols, rows = 3, 2
    pad = 28
    header_h = 280
    grid_top = header_h + 24
    grid_left = 28
    grid_right = w - 28
    grid_bottom = h - 28
    cell_w = (grid_right - grid_left - pad * (cols - 1)) // cols
    cell_h = (grid_bottom - grid_top - pad * (rows - 1)) // rows

    cmds = [
        ("STATUS", "status"),
        ("DISARM", "disarm"),
        ("ARM NIGHT", "arm night"),
        ("ARM AWAY", "arm away"),
        ("LOCK ALL", "lock all"),
        ("UNLOCK ALL", "unlock all"),
    ]

    areas: List[Dict[str, Any]] = []
    for i, (label, cmd) in enumerate(cmds):
        r = i // cols
        c = i % cols
        x = grid_left + c * (cell_w + pad)
        y = grid_top + r * (cell_h + pad)
        bounds = {"x": int(x), "y": int(y), "width": int(cell_w), "height": int(cell_h)}
        action = {
            "type": "postback",
            "label": label[:20],
            "data": f"cmd={cmd}",
        }
        areas.append({"bounds": bounds, "action": action})

    payload = {
        "size": {"width": w, "height": h},
        "selected": True,
        "name": "SecurityHome",
        "chatBarText": "SecurityHome",
        "areas": areas,
    }
    return payload, cmds


def generate_ui_image_2500x1686(out_path: str, mode_label: str) -> None:
    W, H = 2500, 1686
    img = Image.new("RGB", (W, H), (245, 244, 240))
    d = ImageDraw.Draw(img)

    font_paths = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf",
    ]
    font_path = next((p for p in font_paths if os.path.exists(p)), None)
    if font_path:
        f_title = ImageFont.truetype(font_path, 96)
        f_sub = ImageFont.truetype(font_path, 54)
        f_btn = ImageFont.truetype(font_path, 64)
        f_small = ImageFont.truetype(font_path, 40)
    else:
        f_title = f_sub = f_btn = f_small = ImageFont.load_default()

    header_h = 280
    header = (28, 28, W - 28, header_h)
    d.rounded_rectangle(header, radius=36, fill=(20, 24, 28))
    d.text((78, 78), "SecurityHome", font=f_title, fill=(245, 244, 240))
    d.text((78, 178), "Tap a command", font=f_sub, fill=(176, 190, 197))

    # Mode pill (color per mode)
    m = (mode_label or "").strip().upper()
    if m == "AWAY":
        pill_fill = (183, 28, 28)
    elif m == "NIGHT":
        pill_fill = (239, 108, 0)
    else:
        pill_fill = (46, 125, 50)

    pill_w, pill_h = 760, 120
    pill = (W - 78 - pill_w, 110, W - 78, 110 + pill_h)
    d.rounded_rectangle(pill, radius=60, fill=pill_fill)
    d.text((pill[0] + 42, pill[1] + 28), f"MODE: {m}", font=f_sub, fill=(255, 255, 255))

    pad = 28
    grid_top = header_h + 24
    grid_left = 28
    grid_right = W - 28
    grid_bottom = H - 28
    cols, rows = 3, 2
    cell_w = (grid_right - grid_left - pad * (cols - 1)) // cols
    cell_h = (grid_bottom - grid_top - pad * (rows - 1)) // rows

    buttons = [
        ("STATUS", "cmd=status", (86, 132, 166)),
        ("DISARM", "cmd=disarm", (46, 125, 50)),
        ("ARM NIGHT", "cmd=arm night", (239, 108, 0)),
        ("ARM AWAY", "cmd=arm away", (183, 28, 28)),
        ("LOCK ALL", "cmd=lock all", (69, 90, 100)),
        ("UNLOCK ALL", "cmd=unlock all", (96, 125, 139)),
    ]

    for i, (label, cmd, color) in enumerate(buttons):
        r = i // cols
        c = i % cols
        x0 = grid_left + c * (cell_w + pad)
        y0 = grid_top + r * (cell_h + pad)
        x1 = x0 + cell_w
        y1 = y0 + cell_h

        d.rounded_rectangle((x0, y0, x1, y1), radius=42, fill=color)

        bbox = d.textbbox((0, 0), label, font=f_btn)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        d.text((x0 + (cell_w - tw) // 2, y0 + (cell_h - th) // 2 - 18), label, font=f_btn, fill=(255, 255, 255))

        bbox2 = d.textbbox((0, 0), cmd, font=f_small)
        tw2 = bbox2[2] - bbox2[0]
        d.text((x0 + (cell_w - tw2) // 2, y0 + (cell_h) // 2 + 58), cmd, font=f_small, fill=(235, 235, 235))

        d.rounded_rectangle((x0, y0, x1, y1), radius=42, outline=(245, 244, 240), width=6)

    img.save(out_path, "PNG")


def upsert_env_kv(env_path: str, kv: Dict[str, str]) -> None:
    lines: List[str] = []
    if os.path.exists(env_path):
        with open(env_path, "r", encoding="utf-8") as f:
            lines = f.read().splitlines()

    out: List[str] = []
    seen = set()
    for line in lines:
        raw = line
        s = line.strip()
        if not s or s.startswith("#") or "=" not in s:
            out.append(raw)
            continue
        k, _ = s.split("=", 1)
        k = k.strip()
        if k in kv:
            out.append(f"{k}={kv[k]}")
            seen.add(k)
        else:
            out.append(raw)
    for k, v in kv.items():
        if k not in seen:
            out.append(f"{k}={v}")

    with open(env_path, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default="ui_richmenu_mock_2500x1686.png", help="PNG path (2500x1686)")
    ap.add_argument("--delete-old-default", action="store_true", help="Delete current default rich menu (if any)")
    ap.add_argument("--create-mode-set", action="store_true", help="Create 3 mode images+menus (DISARM/NIGHT/AWAY)")
    ap.add_argument("--write-env", action="store_true", help="Write LINE_RICHMENU_ID_* into .env")
    args = ap.parse_args()

    load_env_file(".env")
    access_token = env("LINE_CHANNEL_ACCESS_TOKEN")
    if not access_token:
        raise SystemExit("LINE_CHANNEL_ACCESS_TOKEN missing in .env")

    if args.create_mode_set:
        # Generate images next to this script.
        img_disarm = os.path.join(os.getcwd(), "ui_richmenu_disarm_2500x1686.png")
        img_night = os.path.join(os.getcwd(), "ui_richmenu_night_2500x1686.png")
        img_away = os.path.join(os.getcwd(), "ui_richmenu_away_2500x1686.png")
        generate_ui_image_2500x1686(img_disarm, "DISARM")
        generate_ui_image_2500x1686(img_night, "NIGHT")
        generate_ui_image_2500x1686(img_away, "AWAY")

        payload, cmds = build_richmenu_2500x1686()
        ids: Dict[str, str] = {}
        for mode, img_path in [("DISARM", img_disarm), ("NIGHT", img_night), ("AWAY", img_away)]:
            res = api_post_json("https://api.line.me/v2/bot/richmenu", access_token, payload)
            richmenu_id = str(res.get("richMenuId", "") or "")
            if not richmenu_id:
                raise RuntimeError(f"richMenuId missing for {mode}, response={json.dumps(res)[:300]}")
            upload_richmenu_image(richmenu_id, access_token, img_path)
            ids[mode] = richmenu_id

        # Set default to DISARM version.
        set_default_richmenu(ids["DISARM"], access_token)

        print("richMenuId_disarm:", ids["DISARM"])
        print("richMenuId_night:", ids["NIGHT"])
        print("richMenuId_away:", ids["AWAY"])
        print("buttons:", ", ".join([c for _, c in cmds]))

        if args.write_env:
            upsert_env_kv(
                os.path.join(os.getcwd(), ".env"),
                {
                    "LINE_RICHMENU_ID_DISARM": ids["DISARM"],
                    "LINE_RICHMENU_ID_NIGHT": ids["NIGHT"],
                    "LINE_RICHMENU_ID_AWAY": ids["AWAY"],
                },
            )
            print("wrote .env keys: LINE_RICHMENU_ID_DISARM/ NIGHT/ AWAY")
        return 0

    png_path = args.image
    if not os.path.isabs(png_path):
        png_path = os.path.join(os.getcwd(), png_path)
    if not os.path.exists(png_path):
        raise SystemExit(f"image not found: {png_path}")

    old_default = get_default_richmenu_id(access_token)
    if args.delete_old_default and old_default:
        api_delete(f"https://api.line.me/v2/bot/richmenu/{old_default}", access_token)

    payload, cmds = build_richmenu_2500x1686()
    res = api_post_json("https://api.line.me/v2/bot/richmenu", access_token, payload)
    richmenu_id = str(res.get("richMenuId", "") or "")
    if not richmenu_id:
        raise RuntimeError(f"richMenuId missing, response={json.dumps(res)[:300]}")

    upload_richmenu_image(richmenu_id, access_token, png_path)
    set_default_richmenu(richmenu_id, access_token)

    print("richMenuId:", richmenu_id)
    print("buttons:", ", ".join([c for _, c in cmds]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
