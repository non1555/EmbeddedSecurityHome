# Logic Flowcharts Hierarchy

Logic-only view of the project flow.
Excluded from this document:
- Debounce details
- Sensor health monitoring details

## Level 0: Whole System

```mermaid
graph TD
  U[User and Sensors] --> BR[LINE Bridge]
  BR --> CMD[MQTT Main Command]
  CMD --> MB[Main Board Orchestrator]

  MB --> MSTAT[Main Status]
  MB --> MEVT[Main Event]
  MB --> MACK[Main Ack]
  MB --> MMET[Main Metrics]
  MB --> ACT[Door Window Buzzer]

  MSTAT --> AB[Automation Board]
  AB --> ALIGHT[Light Output]
  AB --> AFAN[Fan Output]
  AB --> ASTAT[Auto Status]
  AB --> AACK[Auto Ack]

  MSTAT --> BR
  MEVT --> BR
  MACK --> BR
  MMET --> BR
  ASTAT --> BR
  AACK --> BR
  BR --> LINE[LINE Reply and Push]
```

## Level 1: Main Board Major Flow

```mermaid
graph TD
  TICK[Orchestrator Tick] --> REMOTE_Q{Remote Command Available}
  REMOTE_Q -->|yes| REMOTE[Remote Command Branch]
  REMOTE_Q -->|no| KEYPAD_Q{Keypad Event Available}

  REMOTE --> KEYPAD_Q
  KEYPAD_Q -->|yes| KEYPAD[Keypad Branch]
  KEYPAD_Q -->|no| TIMEOUT_Q{Entry Timeout Event}

  KEYPAD --> TIMEOUT_Q
  TIMEOUT_Q -->|yes| RULE[Rule Engine]
  TIMEOUT_Q -->|no| SENSOR_Q{Sensor Manual Serial Event}

  SENSOR_Q -->|yes| SERIAL_MANUAL[Serial and Manual Branch]
  SENSOR_Q -->|no| END[Tick End]

  SERIAL_MANUAL --> RULE
  RULE --> OUTPUT[Command Dispatcher and Publish]
  OUTPUT --> END
```

## Level 2.1: Main Board Remote Command Branch

```mermaid
graph TD
  IN[Remote Payload] --> PARSE[Parse and Normalize Command]
  PARSE --> AUTH_OK{Auth and Nonce Valid}
  AUTH_OK -->|no| FAIL[Ack Fail and Status]
  AUTH_OK -->|yes| CLASSIFY{Command Class}

  CLASSIFY -->|status| C_STATUS[Build Status Reply]
  CLASSIFY -->|buzzer| C_BUZZ[Buzzer Action]
  CLASSIFY -->|mode| C_MODE[Emit Mode Event]
  CLASSIFY -->|lock| C_LOCK[Lock Policy Check]
  CLASSIFY -->|unlock| C_UNLOCK[Unlock Policy Check]
  CLASSIFY -->|unknown| C_UNKNOWN[Unknown Command]

  C_MODE --> RULE_IN[To Rule Engine]
  C_STATUS --> ACK_OK[Ack Success and Status]
  C_BUZZ --> ACK_OK
  C_LOCK --> ACK_OK
  C_UNLOCK --> ACK_OK
  C_UNKNOWN --> FAIL
  RULE_IN --> ACK_OK
```

## Level 3.1: Main Board Unlock Policy

```mermaid
graph TD
  REQ[Unlock Door Window All] --> MODE_OK{Mode is Disarm}
  MODE_OK -->|no| REJECT_MODE[Reject Unlock]
  MODE_OK -->|yes| FAULT_OK{Sensor Fault Allows Unlock}
  FAULT_OK -->|no| REJECT_FAULT[Reject Unlock]
  FAULT_OK -->|yes| APPLY[Apply Unlock]
  APPLY --> SESSION[Start Door Unlock Session If Needed]
  SESSION --> DONE[Success Status]
```

## Level 2.2: Main Board Keypad Branch

```mermaid
graph TD
  KIN[Keypad Event] --> KTYPE{Event Type}

  KTYPE -->|hold_warn_silence| KSIL[Silence Hold Warning]
  KTYPE -->|door_code_bad| KBAD[Bad Code Path]
  KTYPE -->|door_code_unlock| KUNLOCK[Unlock Code Path]
  KTYPE -->|mode event| KMODE[Mode Event Path]
  KTYPE -->|other| KBLOCK[Ignore Unsupported Keypad Command]

  KBAD --> KLOCKOUT{Keypad Lockout Active}
  KLOCKOUT -->|yes| KREJECT[Reject]
  KLOCKOUT -->|no| KINC[Increment Bad Attempt]
  KINC --> KLIMIT{Reached Bad Limit}
  KLIMIT -->|yes| KSET[Set Lockout and Alert]
  KLIMIT -->|no| KFAIL[Bad Attempt Status]

  KUNLOCK --> KLOCK2{Keypad Lockout Active}
  KLOCK2 -->|yes| KREJECT
  KLOCK2 -->|no| KPOLICY[Unlock Policy]
  KPOLICY --> KAPPLY[Disarm If Needed and Unlock Door]

  KMODE --> KRULE[To Rule Engine]
```

## Level 2.3: Main Board Serial and Manual Branch

```mermaid
graph TD
  EIN[Sensor Manual Serial Event] --> ETYPE{Event Source}

  ETYPE -->|serial mode| SG_MODE{Serial Mode Allowed}
  ETYPE -->|serial manual| SG_MAN{Serial Manual Allowed}
  ETYPE -->|serial sensor| SG_SEN{Serial Sensor Allowed}
  ETYPE -->|manual button| MAN[Manual Actuator Event]
  ETYPE -->|real sensor| RSEN[Sensor Event]

  SG_MODE -->|no| SBLOCK1[Publish Serial Blocked Status]
  SG_MODE -->|yes| SRULE1[To Rule Engine]
  SG_MAN -->|no| SBLOCK2[Publish Serial Blocked Status]
  SG_MAN -->|yes| MAN
  SG_SEN -->|no| SBLOCK3[Publish Serial Blocked Status]
  SG_SEN -->|yes| SRULE2[To Rule Engine]

  MAN --> MPOL{Manual Lock or Unlock Request}
  MPOL --> MLOCK[Lock Requires Door and Window Closed]
  MPOL --> MUNLOCK[Unlock Requires Disarm and Fault Allow]
  MLOCK --> MOUT[Apply Servo and Publish]
  MUNLOCK --> MOUT

  RSEN --> SRULE3[To Rule Engine]
```

## Level 2.4: Main Board Rule Engine

```mermaid
graph TD
  EV[Event Input] --> CLASS{Event Class}

  CLASS -->|disarm| R_DISARM[Set Mode Disarm and Reset]
  CLASS -->|arm_night| R_NIGHT[Set Mode Night and Reset]
  CLASS -->|arm_away| R_AWAY[Set Mode Away and Reset]
  CLASS -->|door_open| R_DOOR[Entry Pending and Warn]
  CLASS -->|entry_timeout| R_TIMEOUT[Escalate Critical]
  CLASS -->|window_open| R_WIN[Raise Suspicion]
  CLASS -->|motion or chokepoint| R_MOTION[Raise Suspicion]
  CLASS -->|vibration| R_VIB[Raise Suspicion]
  CLASS -->|door_tamper| R_TAMPER[Raise Suspicion Fast]
  CLASS -->|other| R_OTHER[No State Change]

  R_DISARM --> SCORE[Update Suspicion and Level]
  R_NIGHT --> SCORE
  R_AWAY --> SCORE
  R_DOOR --> SCORE
  R_TIMEOUT --> SCORE
  R_WIN --> SCORE
  R_MOTION --> SCORE
  R_VIB --> SCORE
  R_TAMPER --> SCORE
  R_OTHER --> SCORE

  SCORE --> CMD[Derive Command Type]
  CMD --> OUT[To Command Dispatcher]
```

## Level 2.5: Main Board Output Dispatcher

```mermaid
graph TD
  DIN[Decision Input] --> MODEPOL[Mode Based Lock Policy]
  MODEPOL --> CMDPOL[Command Type Policy]

  CMDPOL -->|none| NOP[No Extra Action]
  CMDPOL -->|buzzer_warn| BWARN[Buzzer Warn]
  CMDPOL -->|buzzer_alert| BALERT[Buzzer Alert]
  CMDPOL -->|servo_lock| SLOCK[Servo Lock]
  CMDPOL -->|notify| NOTI[Notify Text]

  MODEPOL --> SERVO[Servo State]
  BWARN --> BUZZ[Buzzer State]
  BALERT --> BUZZ
  SLOCK --> SERVO

  SERVO --> PUB[Publish Status Event Ack Metrics]
  BUZZ --> PUB
  NOTI --> PUB
  NOP --> PUB
```

## Level 1: Automation Board Major Flow

```mermaid
graph TD
  BOOT[Runtime Start] --> NET[Task Net]
  BOOT --> CTRL[Task Control]

  NET --> NLOOP[Network Loop]
  NLOOP --> MRX[MQTT Message Branch]
  NLOOP --> NPUB[Periodic Auto Status]

  CTRL --> CYCLE[Control Cycle]
  CYCLE --> LIGHT[Light Auto Branch]
  CYCLE --> FAN[Fan Auto Branch]
  LIGHT --> APPLY[Apply Outputs]
  FAN --> APPLY
  APPLY --> APUB[Publish Auto Status Ack]
```

## Level 2.6: Automation MQTT Message Branch

```mermaid
graph TD
  MX[MQTT Message] --> TOP{Topic Type}

  TOP -->|main status| MAINCTX[Parse Main Mode and Presence]
  MAINCTX --> CTXOK{Valid Context Fields}
  CTXOK -->|yes| CTXWRITE[Write Main Context Timestamp]
  CTXOK -->|no| IGN1[Ignore Invalid Context]

  TOP -->|auto cmd| ACMD[Parse Authorized Auto Command]
  ACMD --> AOK{Auth Valid}
  AOK -->|no| AFAIL[Publish Ack Fail and Status]
  AOK -->|yes| ACLASS{Command Class}

  ACLASS -->|light auto on off| LWRITE[Write Light Auto State]
  ACLASS -->|fan auto on off| FWRITE[Write Fan Auto State]
  ACLASS -->|status| ASTATUS[Publish Current Status]
  ACLASS -->|unknown| AUNK[Publish Unknown Ack]

  LWRITE --> APPLY1[Apply GPIO Outputs]
  FWRITE --> APPLY1
  APPLY1 --> AOKPUB[Publish Ack and Status]
  ASTATUS --> AOKPUB
  AUNK --> AFAIL
```

## Level 2.7: Automation Control Loop

```mermaid
graph TD
  LOOP[Control Loop Tick] --> LUX_DUE{Lux Sample Due}
  LUX_DUE -->|yes| LUX_READ[Read Lux]
  LUX_DUE -->|no| TEMP_DUE

  LUX_READ --> L_GATE_MODE{Allow by Main Mode}
  L_GATE_MODE -->|no| L_FORCE_OFF[Force Light Off By Main Gate]
  L_GATE_MODE -->|yes| L_GATE_HOME{Allow by Main Presence}
  L_GATE_HOME -->|no| L_FORCE_OFF
  L_GATE_HOME -->|yes| L_NEXT[Compute Light Hysteresis]
  L_NEXT --> L_CHG{Light Changed}
  L_CHG -->|yes| L_APPLY[Apply Light Output]
  L_CHG -->|no| L_KEEP

  TEMP_DUE{Temp Sample Due} -->|yes| TEMP_READ[Read Temp Hum]
  TEMP_DUE -->|no| END[Loop End]
  TEMP_READ --> F_GATE_MODE{Allow by Main Mode}
  F_GATE_MODE -->|no| F_FORCE_OFF[Force Fan Off By Main Gate]
  F_GATE_MODE -->|yes| F_GATE_HOME{Allow by Main Presence}
  F_GATE_HOME -->|no| F_FORCE_OFF
  F_GATE_HOME -->|yes| F_NEXT[Compute Fan Hysteresis]
  F_NEXT --> F_CHG{Fan Changed}
  F_CHG -->|yes| F_APPLY[Apply Fan Output]
  F_CHG -->|no| F_KEEP
  L_APPLY --> END
  L_FORCE_OFF --> L_APPLY
  F_APPLY --> END
  F_FORCE_OFF --> F_APPLY
  F_KEEP --> END
```

## Level 3.2: Light Auto Decision

```mermaid
graph TD
  LIN[Lux and Main Context] --> MODE_OK{Mode Allows Auto}
  MODE_OK -->|no| L_OFF_GATE[Force Light Off]
  MODE_OK -->|yes| HOME_OK{Presence Allows Auto}
  HOME_OK -->|no| L_OFF_GATE
  HOME_OK -->|yes| TH_LOW{Lux Below On Threshold}
  TH_LOW -->|yes| L_ON[Set Light On]
  TH_LOW -->|no| TH_HIGH{Lux Above Off Threshold}
  TH_HIGH -->|yes| L_OFF[Set Light Off]
  TH_HIGH -->|no| L_HOLD[Hold Light State]
```

## Level 3.3: Fan Auto Decision

```mermaid
graph TD
  FIN[Temp and Main Context] --> MODE_OK{Mode Allows Auto}
  MODE_OK -->|no| F_OFF_GATE[Force Fan Off]
  MODE_OK -->|yes| HOME_OK{Presence Allows Auto}
  HOME_OK -->|no| F_OFF_GATE
  HOME_OK -->|yes| F_ON_TH{Above Fan On Threshold}
  F_ON_TH -->|yes| F_ON[Set Fan On]
  F_ON_TH -->|no| F_OFF_TH{Below Fan Off Threshold}
  F_OFF_TH -->|yes| F_OFF[Set Fan Off]
  F_OFF_TH -->|no| F_HOLD[Hold Fan State]
```

## Level 1: LINE Bridge Major Flow

```mermaid
graph TD
  RX[Webhook or HTTP Command] --> CMD_CHECK{Supported Command}
  CMD_CHECK -->|no| REJ[Reject]
  CMD_CHECK -->|yes| AUTH{Mutating Command Auth Ready}
  AUTH -->|no| REJ
  AUTH -->|yes| PUB[Publish MQTT Command]
  PUB --> PUB_OK{Publish Success}
  PUB_OK -->|yes| REPLY[Reply or Push Success]
  PUB_OK -->|no| REJ

  MMSG[MQTT Message] --> TOPIC{Topic Branch}
  TOPIC -->|status| S1[Update Status Snapshot]
  TOPIC -->|event| S2[Update Event Snapshot]
  TOPIC -->|ack| S3[Update Ack Snapshot]
  TOPIC -->|metrics| S4[Throttle Metrics Message]
  S1 --> PUSH[Push LINE Message]
  S2 --> PUSH
  S3 --> PUSH
  S4 --> PUSH
```

## Level 2.8: Bridge Command Ingress

```mermaid
graph TD
  IN[LINE Payload or HTTP Query] --> PARSE[Parse Command]
  PARSE --> SUP{Supported}
  SUP -->|no| OUT_REJ[Reject Reply]
  SUP -->|yes| AUTH{Auth Required and Valid}
  AUTH -->|no| OUT_REJ
  AUTH -->|yes| PUB[Publish to Main Command Topic]
  PUB --> OK{Publish OK}
  OK -->|yes| OUT_OK[Reply Accepted]
  OK -->|no| OUT_REJ
```

## Level 2.9: Bridge MQTT Message Fanout

```mermaid
graph TD
  INM[MQTT Topic Payload] --> BRANCH{Topic Type}
  BRANCH -->|main status| U1[Update Home Snapshot]
  BRANCH -->|main event| U2[Update Last Event Snapshot]
  BRANCH -->|main ack| U3[Update Lock State Snapshot]
  BRANCH -->|main metrics| U4[Rate Limit Metrics Text]
  BRANCH -->|auto status| U5[Update Auto Snapshot]
  BRANCH -->|auto ack| U6[Update Auto Ack Snapshot]
  U1 --> OUT[Push LINE Text]
  U2 --> OUT
  U3 --> OUT
  U4 --> OUT
  U5 --> OUT
  U6 --> OUT
```
