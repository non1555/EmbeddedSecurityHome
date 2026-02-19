from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


OUT_DIR = Path(__file__).resolve().parent


def font(size: int, bold: bool = False):
  candidates = [
    "segoeuib.ttf" if bold else "segoeui.ttf",
    "arialbd.ttf" if bold else "arial.ttf",
  ]
  for name in candidates:
    try:
      return ImageFont.truetype(name, size)
    except OSError:
      continue
  return ImageFont.load_default()


def draw_box(draw, x, y, w, h, title, lines, accent=(49, 199, 176), fill=(245, 245, 245)):
  draw.rounded_rectangle((x + 14, y + 14, x + w + 14, y + h + 14), radius=18, fill=accent)
  draw.rounded_rectangle((x, y, x + w, y + h), radius=18, fill=fill, outline=(30, 30, 30), width=4)
  draw.text((x + 22, y + 18), title, fill=(20, 20, 20), font=font(30, True))
  ty = y + 62
  for line in lines:
    draw.text((x + 22, ty), line, fill=(30, 30, 30), font=font(22))
    ty += 30


def arrow(draw, pts, width=5):
  draw.line(pts, fill=(20, 20, 20), width=width)
  x1, y1, x2, y2 = pts[-4], pts[-3], pts[-2], pts[-1]
  if x2 == x1:
    if y2 > y1:
      tri = [(x2, y2), (x2 - 10, y2 - 16), (x2 + 10, y2 - 16)]
    else:
      tri = [(x2, y2), (x2 - 10, y2 + 16), (x2 + 10, y2 + 16)]
  else:
    if x2 > x1:
      tri = [(x2, y2), (x2 - 16, y2 - 10), (x2 - 16, y2 + 10)]
    else:
      tri = [(x2, y2), (x2 + 16, y2 - 10), (x2 + 16, y2 + 10)]
  draw.polygon(tri, fill=(20, 20, 20))


def block_diagram_png():
  img = Image.new("RGB", (1600, 920), (239, 239, 239))
  draw = ImageDraw.Draw(img)

  draw.text((24, 22), "PROJECT SYSTEM", fill=(20, 20, 20), font=font(58, True))
  draw.rounded_rectangle((560, 22, 1070, 94), radius=18, fill=(49, 199, 176))
  draw.text((592, 39), "BLOCK DIAGRAM", fill=(255, 255, 255), font=font(34, True))

  draw_box(
    draw, 56, 186, 370, 168, "Security Sensors",
    ["Reed, PIR, Vibration, Ultrasonic,", "Keypad, Manual Buttons"],
  )
  draw_box(
    draw, 56, 504, 370, 132, "Automation Sensors",
    ["BH1750 (Lux), DHT (Temp/Hum)"],
  )
  draw_box(draw, 588, 34, 390, 96, "MQTT Broker", [])
  draw_box(
    draw, 588, 196, 390, 192, "Main Board (ESP32)",
    ["Security Orchestrator", "Rule Engine, Timeout, Door Session", "Lock/Alarm Policy + Status Publish"],
  )
  draw_box(
    draw, 588, 516, 390, 166, "Automation Board (ESP32)",
    ["Light/Fan Automation Pipeline", "Presence Gate + Hysteresis Control"],
  )
  draw_box(
    draw, 1106, 148, 390, 154, "LINE Bridge",
    ["FastAPI + MQTT + Line API", "Command/Auth/Notify Gateway"],
  )
  draw_box(draw, 1152, 366, 288, 102, "LINE OA / User", [])
  draw_box(
    draw, 1106, 548, 390, 220, "Actuators",
    ["Door lock / Window lock (Servo)", "Buzzer + OLED (Main Board)", "Light LED + Fan via L293D", "(Automation Board)"],
  )

  arrow(draw, [427, 270, 580, 270])
  arrow(draw, [427, 568, 580, 604])
  arrow(draw, [782, 130, 782, 194])
  arrow(draw, [782, 388, 782, 516])
  arrow(draw, [981, 86, 1102, 220])
  arrow(draw, [1102, 236, 981, 102])
  arrow(draw, [1439, 434, 1490, 434, 1490, 316, 1439, 316])
  arrow(draw, [1439, 300, 1490, 300, 1490, 418, 1439, 418])
  arrow(draw, [980, 286, 1102, 648])
  arrow(draw, [980, 616, 1102, 704])

  img.save(OUT_DIR / "block_diagram_presentation.png")


def pin_allocation_png():
  img = Image.new("RGB", (1600, 1100), (240, 240, 240))
  draw = ImageDraw.Draw(img)
  draw.text((36, 20), "PIN ALLOCATION OVERVIEW", fill=(20, 20, 20), font=font(52, True))
  draw.text(
    (38, 90),
    "Source: src/main_board/app/HardwareConfig.h and src/auto_board/hardware/AutoHardwareConfig.h",
    fill=(40, 40, 40), font=font(18)
  )

  # Left panel (main board)
  draw.rounded_rectangle((48, 142, 778, 1050), radius=18, fill=(217, 245, 239), outline=(30, 30, 30), width=3)
  draw.rounded_rectangle((48, 142, 778, 212), radius=18, fill=(44, 191, 168))
  draw.text((304, 166), "MAIN BOARD (ESP32)", fill=(255, 255, 255), font=font(32, True))

  rows_left = [
    ("Buzzer", "25"),
    ("Servo Door Lock", "26"),
    ("Servo Window Lock", "27"),
    ("Reed Door", "32"),
    ("Reed Window", "19"),
    ("PIR 1", "35"),
    ("PIR 2", "36"),
    ("PIR 3 (Outdoor)", "39"),
    ("Vibration Combined", "34"),
    ("Ultrasonic #1 TRIG / ECHO", "13 / 14"),
    ("Ultrasonic #2 TRIG / ECHO", "16 / 17"),
    ("Ultrasonic #3 TRIG / ECHO", "4 / 5"),
    ("Manual Door / Window Button", "33 / 18"),
    ("I2C SDA / SCL", "21 / 22"),
  ]
  colors = [
    (255, 245, 207), (231, 243, 255), (231, 243, 255), (234, 255, 234), (234, 255, 234),
    (244, 234, 255), (244, 234, 255), (244, 234, 255), (255, 231, 238),
    (227, 251, 255), (227, 251, 255), (227, 251, 255), (255, 241, 225), (241, 255, 241),
  ]

  draw.rectangle((84, 236, 584, 288), fill=(255, 227, 159), outline=(68, 68, 68), width=1)
  draw.rectangle((584, 236, 744, 288), fill=(255, 211, 167), outline=(68, 68, 68), width=1)
  draw.text((106, 253), "Function", fill=(30, 30, 30), font=font(24, True))
  draw.text((640, 253), "GPIO", fill=(30, 30, 30), font=font(24, True))

  y = 288
  for i, (name, gpio) in enumerate(rows_left):
    fill = colors[i % len(colors)]
    draw.rectangle((84, y, 584, y + 50), fill=fill, outline=(85, 85, 85), width=1)
    draw.rectangle((584, y, 744, y + 50), fill=fill, outline=(85, 85, 85), width=1)
    draw.text((100, y + 14), name, fill=(30, 30, 30), font=font(22))
    draw.text((620, y + 14), gpio, fill=(30, 30, 30), font=font(22, True))
    y += 50

  # Right panel (automation)
  draw.rounded_rectangle((824, 142, 1552, 1050), radius=18, fill=(223, 240, 255), outline=(30, 30, 30), width=3)
  draw.rounded_rectangle((824, 142, 1552, 212), radius=18, fill=(59, 134, 216))
  draw.text((1042, 166), "AUTOMATION BOARD (ESP32)", fill=(255, 255, 255), font=font(32, True))

  draw.rectangle((860, 236, 1360, 288), fill=(255, 227, 159), outline=(68, 68, 68), width=1)
  draw.rectangle((1360, 236, 1516, 288), fill=(255, 211, 167), outline=(68, 68, 68), width=1)
  draw.text((882, 253), "Function", fill=(30, 30, 30), font=font(24, True))
  draw.text((1410, 253), "GPIO", fill=(30, 30, 30), font=font(24, True))

  rows_right = [
    ("Light LED Output", "27", (255, 245, 207)),
    ("L293D IN1 (Fan)", "25", (255, 231, 238)),
    ("L293D IN2 (Fan)", "26", (255, 231, 238)),
    ("L293D EN (Optional)", "33", (255, 231, 238)),
    ("DHT Data", "32", (234, 255, 234)),
    ("I2C SDA / SCL", "21 / 22", (241, 255, 241)),
    ("BH1750 I2C Address (Primary)", "0x23", (227, 251, 255)),
    ("BH1750 I2C Address (Secondary)", "0x5C", (227, 251, 255)),
  ]
  y = 288
  for name, gpio, fill in rows_right:
    draw.rectangle((860, y, 1360, y + 58), fill=fill, outline=(85, 85, 85), width=1)
    draw.rectangle((1360, y, 1516, y + 58), fill=fill, outline=(85, 85, 85), width=1)
    draw.text((878, y + 18), name, fill=(30, 30, 30), font=font(22))
    draw.text((1410, y + 18), gpio, fill=(30, 30, 30), font=font(22, True))
    y += 58

  draw.rounded_rectangle((860, 772, 1516, 1016), radius=12, fill=(255, 255, 255), outline=(46, 93, 143), width=2)
  draw.text((886, 806), "Automation Thresholds (Current)", fill=(30, 30, 30), font=font(24, True))
  draw.text((890, 850), "Light: ON if lux < 120, OFF if lux > 180", fill=(30, 30, 30), font=font(22))
  draw.text((890, 892), "Fan:   ON if temp >= 20 C, OFF if temp <= 15 C", fill=(30, 30, 30), font=font(22))
  draw.text((890, 934), "Sampling: Light 400 ms, Temp 2000 ms", fill=(30, 30, 30), font=font(22))
  draw.text((890, 976), "Defined in AutoHardwareConfig.h", fill=(50, 50, 50), font=font(18))

  img.save(OUT_DIR / "pin_allocation_overview.png")


def waterfall_png():
  img = Image.new("RGB", (1300, 930), (239, 239, 239))
  draw = ImageDraw.Draw(img)
  draw.text((34, 22), "PROJECT WATERFALL: PLAN TO FINAL WORK", fill=(20, 20, 20), font=font(50, True))

  boxes = [
    (42, 118, 220, 96, (255, 244, 207), (195, 165, 80), "Planning", None),
    (150, 236, 240, 110, (255, 228, 228), (192, 106, 106), "Requirements", "and Scope"),
    (300, 378, 235, 104, (239, 227, 255), (144, 112, 175), "Architecture", None),
    (442, 520, 280, 120, (220, 234, 215), (124, 174, 103), "Implementation", "Main + Auto + Bridge"),
    (590, 670, 220, 104, (220, 233, 251), (111, 146, 196), "Integration", None),
    (760, 752, 230, 108, (242, 242, 242), (127, 127, 127), "Testing", None),
    (910, 784, 220, 100, (255, 240, 216), (216, 160, 62), "Tuning", None),
    (1040, 620, 250, 130, (255, 224, 222), (220, 122, 114), "Final Work", "Docs + Demo\n+ Handoff"),
  ]
  for x, y, w, h, fill, outline, t1, t2 in boxes:
    draw.rounded_rectangle((x, y, x + w, y + h), radius=16, fill=fill, outline=outline, width=6)
    draw.text((x + 38, y + 34), t1, fill=(35, 35, 35), font=font(36, True))
    if t2:
      for i, line in enumerate(t2.split("\n")):
        draw.text((x + 32, y + 68 + (i * 28)), line, fill=(45, 45, 45), font=font(24))

  arrow(draw, [262, 166, 320, 166, 320, 236])
  arrow(draw, [390, 292, 450, 292, 450, 378])
  arrow(draw, [535, 430, 584, 430, 584, 520])
  arrow(draw, [722, 580, 744, 580, 744, 670])
  arrow(draw, [810, 724, 836, 724, 836, 752])
  arrow(draw, [990, 806, 1020, 806, 1020, 784])
  arrow(draw, [1130, 834, 1190, 834, 1190, 740, 1040, 740])

  draw.text(
    (70, 894),
    "Flow style follows the example submission: simple, high-contrast, staircase progression.",
    fill=(45, 45, 45), font=font(24)
  )
  img.save(OUT_DIR / "waterfall_plan_to_final.png")


def main():
  block_diagram_png()
  pin_allocation_png()
  waterfall_png()
  print("Generated PNG files in", OUT_DIR)


if __name__ == "__main__":
  main()
