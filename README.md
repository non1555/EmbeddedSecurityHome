# EmbeddedSecurityHome

คู่มือสำหรับผู้ใช้งานทั่วไป (ไม่ต้องแก้โค้ด, ไม่ต้องใช้ VS Code)

## เป้าหมาย

หลังทำครบคู่มือนี้ คุณจะสามารถ:

1. เปิด UI Launcher ได้
2. กรอกค่าต่างๆ ใน UI ได้
3. กดปุ่ม Deploy เพื่ออัปโหลดเฟิร์มแวร์ได้
4. ใช้ LINE ควบคุมระบบได้

## 1) ติดตั้งโปรแกรมที่ต้องมี (ครั้งเดียว)

1. ติดตั้ง Python 3.10+
- https://www.python.org/downloads/
- ตอนติดตั้ง Windows ให้ติ๊ก `Add python.exe to PATH`

2. ติดตั้ง Git
- https://git-scm.com/downloads

3. ติดตั้ง ngrok + สมัครบัญชี
- สมัคร: https://dashboard.ngrok.com/signup
- ดาวน์โหลด: https://ngrok.com/downloads
- เอา Authtoken จาก: https://dashboard.ngrok.com/get-started/your-authtoken

4. สมัคร LINE Developers
- https://developers.line.biz/

5. ติดตั้ง MQTT Broker (ถ้ายังไม่มี)
- Mosquitto: https://mosquitto.org/download/

6. ติดตั้งไดรเวอร์ USB ของบอร์ด ESP32 (ถ้าคอมไม่เห็นพอร์ต)
- CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- CH340: http://www.wch-ic.com/downloads/CH341SER_EXE.html

## 2) ดาวน์โหลดโปรเจกต์

```bash
git clone <repo-url>
cd EmbeddedSecurityHome
```

## 3) ติดตั้งโปรเจกต์แบบปุ่มเดียว

Windows:

```bat
setup.cmd
```

Linux:

```bash
chmod +x setup.sh
./setup.sh
```

Linux (แบบกดจาก UI ไม่ต้องพิมพ์คำสั่ง):

1. ดับเบิลคลิก `tools/linux_ui/install_desktop_launchers.sh` แล้วเลือก Run
2. จะได้ shortcut บน Desktop:
- `securityhome-setup.desktop`
- `securityhome-line-bridge-start.desktop`
- `securityhome-line-bridge-stop.desktop`
- `securityhome-native-tests.desktop`

## 4) เปิด UI Launcher

Windows (แนะนำ):

```bat
tools\line_bridge\start-ui.cmd
```

หรือดับเบิลคลิก `tools/line_bridge/launcher.vbs`

Linux:

```bash
tools/line_bridge/.venv/bin/python3 tools/line_bridge/launcher.pyw
```

Linux (แบบกดจาก UI):

- ดับเบิลคลิก `securityhome-line-bridge-start.desktop`

## 5) ตั้งค่าใน UI (ครั้งแรก)

### 5.1 เตรียมค่าจาก LINE Developers

ไปที่ https://developers.line.biz/ แล้วทำตามนี้:

1. Login
2. Create Provider
3. Create Channel -> เลือก `Messaging API`
4. เข้าเมนู Channel > แท็บ `Messaging API`
5. เอาค่าต่อไปนี้:
- `LINE_CHANNEL_ACCESS_TOKEN` (long-lived)
- `LINE_CHANNEL_SECRET`

### 5.2 กรอกแท็บ Config

ใน Launcher แท็บ `Config`:

1. ใส่ `LINE_CHANNEL_ACCESS_TOKEN`
2. ใส่ `LINE_CHANNEL_SECRET`
3. ใส่ `NGROK_AUTHTOKEN`
4. กด `Save .env`

### 5.3 กรอกแท็บ Firmware

ใน Launcher แท็บ `Firmware`:

1. ใส่ Wi-Fi SSID/Password ที่บอร์ดจะใช้เชื่อมต่อ
2. ใส่ MQTT Broker/Port
- ตัวอย่างถ้า broker อยู่เครื่องเดียวกับ Launcher:
  - `FW_MQTT_BROKER = 192.168.x.x` (IP เครื่องคุณ)
  - `FW_MQTT_PORT = 1883`
3. ใส่ `Door Code (4 digits)` สำหรับรหัสปลดล็อกผ่าน keypad
4. กด `Save to .env`

หมายเหตุ: ไม่ต้องแก้ไฟล์โค้ดหรือ `platformio.ini`

## 6) อัปโหลดเฟิร์มแวร์ผ่าน UI

มี 2 บอร์ดหลัก:

- Main board -> ปุ่ม `Deploy Main Board`
- Automation board -> ปุ่ม `Deploy Automation`

ขั้นตอน:

1. เสียบบอร์ดผ่าน USB
2. กด `Refresh Ports`
3. กดปุ่ม Deploy ตามบอร์ด
4. รอจนขึ้น Upload completed

## 7) เชื่อม LINE Webhook ผ่าน UI

1. แท็บ `Status` กด `Start`
2. กด `Copy Webhook`
3. ไป LINE Developers > Messaging API > Webhook settings
4. วาง URL ที่คัดลอก (`.../line/webhook`)
5. กด Verify และเปิด Use webhook

## 8) ทดสอบใช้งานจริง

1. เพิ่ม OA เป็นเพื่อนใน LINE
2. ส่ง `status`
3. ถ้าระบบตอบกลับได้ แปลว่าพร้อมใช้งาน

## 9) ปัญหาที่พบบ่อย

1. ข้อความ `Activate.ps1 cannot be loaded`
- ไม่ต้องใช้ activate
- ให้ใช้ `setup.cmd` และ `tools\line_bridge\start-ui.cmd` เท่านั้น

2. อัปโหลดไม่ผ่านเพราะพอร์ต
- กด `Refresh Ports`
- เปลี่ยนสาย USB
- เช็ก Device Manager ว่าพอร์ตขึ้นจริง

3. Verify webhook ไม่ผ่าน
- เช็กว่าใน UI กด `Start` แล้ว
- เช็กว่า ngrok ขึ้นใน Status
- เช็ก URL ลงท้าย `/line/webhook`

## 10) สำหรับผู้พัฒนา (ทางเลือก)

ส่วนนี้ไม่จำเป็นสำหรับผู้ใช้งานทั่วไป:

- VS Code + PlatformIO
- คำสั่ง CLI (`python -m platformio ...`)

ถ้าผู้ใช้ทั่วไป ให้ทำผ่าน UI ตามขั้นตอนด้านบนเท่านั้น
