# Layer Structure (Project-Wide)

โครงสร้างใหม่จัดตามบอร์ดก่อน แล้วแยกชั้นงานภายในแต่ละบอร์ด

## Source Tree

```text
src/
  main_board/
    main.cpp
    app/          # orchestration + domain rules/state/config
    pipelines/    # event collection/gating/timeout flows
    services/     # mqtt bus/client, notify, logger, dispatcher
    sensors/      # sensor adapters
    drivers/      # low-level hardware drivers
    actuators/    # buzzer/servo actuator wrappers
    rtos/         # queues/tasks integration
    ui/           # oled keypad UI

  auto_board/
    automation/   # migrated automation logic (presence/light/temp)
    runtime/
      main.cpp
    hardware/
      AutoHardwareConfig.h
```

## Build Mapping

- `main-board`
  - source: `src/main_board/*`
  - include path: `-Isrc/main_board`
  - MQTT namespace: `esh/main/*`
- `automation-board`
  - source: `src/auto_board/runtime/main.cpp` + `src/auto_board/automation/presence.cpp`
  - include path: `-Isrc/auto_board`
  - MQTT namespace: `esh/auto/*`
  - subscribe main status: `MQTT_TOPIC_MAIN_STATUS`

## Auto Board Layers

- Layer `Runtime` (`src/auto_board/runtime/*`)
  - responsibility: process lifecycle, task loop, MQTT transport, command auth/replay guard, orchestration between sensors and outputs
  - owns: `main.cpp`
- Layer `Policy/Use-case` (`src/auto_board/automation/*`)
  - responsibility: reusable automation policy blocks
  - currently active in firmware build: `presence.*`
  - keep `light_system.*` / `temp_system.*` as optional modules (not in default build)
- Layer `Hardware Contract` (`src/auto_board/hardware/AutoHardwareConfig.h`)
  - responsibility: pin map + timing/threshold constants exposed to runtime/policy code
  - rule: no business logic in this file

### Auto Board Dependency Direction

- allowed:
  - `runtime/main.cpp` -> `automation/*`, `hardware/AutoHardwareConfig.h`
  - `automation/*.cpp` -> `automation/*.h`, `hardware/AutoHardwareConfig.h`
- disallowed:
  - `automation/*` calling back into task/network lifecycle internals from `runtime/main.cpp`
  - cross-board include (`auto_board/*` -> `main_board/*`)

## Dependency Rules

- `main_board/app` เป็นชั้นบนสุดของ flow (orchestration) และเป็นจุดรวม policy
- `pipelines/services` ให้ทำหน้าที่ adapter/use-case support; หลีกเลี่ยงใส่ policy ที่ขัดกับ `app`
- `drivers/sensors/actuators/ui/rtos` เป็น infrastructure layer
- `auto_board/runtime` เป็น runtime/control loop ของบอร์ด automation
- `auto_board/automation` เป็น reusable automation policies ของบอร์ด automation
- `auto_board/hardware` เป็น single source of truth สำหรับ pin/threshold config ของบอร์ด automation

## Main vs Auto Layer Comparison

- โครงสร้างที่ "เหมือนกัน"
  - มีชั้น policy/use-case แยกจากค่าฮาร์ดแวร์ (`main_board/app`, `auto_board/automation`)
  - มี runtime entrypoint ชัดเจน (`src/main_board/main.cpp`, `src/auto_board/runtime/main.cpp`)
  - แยก config pin/threshold เป็นไฟล์กลาง (`app/HardwareConfig.h`, `hardware/AutoHardwareConfig.h`)
- โครงสร้างที่ "ต่างกัน"
  - `main_board` แยก infrastructure ละเอียดกว่า (`drivers/sensors/actuators/services/pipelines/rtos/ui`)
  - `auto_board` ยังรวม network + scheduler + control-loop ไว้ใน `runtime/main.cpp` เป็นไฟล์เดียว
- สรุปรูปแบบ
  - รูปแบบหลักเหมือนกัน: `Runtime -> Policy -> Hardware Contract`
  - ระดับ granular ยังไม่เท่ากัน: `main_board` modular กว่า, `auto_board` compact กว่า

## Migration Notes

- ย้ายจากเดิม:
  - `src/main.cpp` -> `src/main_board/main.cpp`
  - `src/{app,pipelines,services,sensors,drivers,actuators,rtos,ui}` -> `src/main_board/...`
  - `src/fw_auto/main.cpp` -> `src/auto_board/runtime/main.cpp`
  - `src/fw_auto/AutoHardwareConfig.h` -> `src/auto_board/hardware/AutoHardwareConfig.h`
  - `src/automation/*` -> `src/auto_board/automation/*`
