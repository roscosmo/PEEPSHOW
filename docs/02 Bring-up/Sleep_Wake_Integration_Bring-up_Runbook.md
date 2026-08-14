# Sleep Wake Integration Bring-up Runbook

This runbook defines HW6 sleep/wake integration validation across Platform subsystems. HW5 results are regression references only.

Related:

- [[Power_and_Sleep_Policy]]
- [[PMIC_and_Power_Contract]]
- [[HW6_Wake_Sources]]
- [[HW6_Power_Rails]]
- [[Display_and_Rendering_Contract]]
- [[HW6_Brought_Up_Tracker]]

---

## Scope

This runbook covers:

- RTC cadence wake
- button wake
- PMIC wake/notification
- joystick threshold wake
- IMU motion/step/gesture wake where enabled
- explicit absence of retired encoder and light-sensor wake paths
- USB attach wake/installer transition
- display static hold across sleep
- owner quiesce/resume sequence
- unknown wake reason classification

---

## Integration Baseline

Sleep/wake bring-up is not a single peripheral test. It proves that all Platform owners can quiesce, enter the selected sleep class, wake for an allowed reason, restore clocks/timebases, and resume ownership without stale hardware state.

The first validated sleep class should be conservative. Deeper sleep classes are added only after wake reason classification and owner resume are reliable.

---

## Owner Quiesce Requirements

| Owner | Before sleep | After wake/resume |
| --- | --- | --- |
| `thPower` | choose sleep class, arm wake sources, record expected wake mask | restore clocks, classify wake, verify timebases |
| `thDisplay` | drain/abort active SPI/LPDMA flush; leave panel hold policy valid | revalidate display path before next flush |
| `thStorage` | block new operations; finish/abort read/program/erase; no USB export surprise | revalidate flash liveness if needed |
| `thAudio` | drain/stop SAI/DMA; `SD_MODE` low unless policy says otherwise | revalidate clocks, SAI, and DMA before playback |
| `thInput` | arm only approved button/joystick wake sources | classify raw wake input and debounce post-wake |
| `thSensor` | configure IMU/TMAG modes according to policy | sample/revalidate sensors that caused wake |
| `thComm` | keep NINA off unless communication mode owns it | do not classify BLE as a wake source unless future policy changes |

Any owner timeout during quiesce is a failed sleep-entry test.

---

## Wake Source Matrix

| Wake source | Intended use | Required evidence |
| --- | --- | --- |
| RTC cadence | ambient display/runtime cadence | wake timestamp, timebase recovery, expected cadence |
| `BTN_START` | primary wake/sleep button | active-low wake classification and debounce |
| A/B/L/R | optional/contextual wake | only wakes when explicitly armed |
| `BTN_BOOT` | maintenance after app boot only | not normal runtime/game input |
| `PMIC_INT` | charger/battery/power events | EXTI15 event and PMIC status readback |
| USB VBUS attach | power and USB host-detection policy | VBUS classification; no MSC prompt/handoff until USB data-host activity or enumeration gate passes |
| TMAG3001 `JOY_INT` | threshold joystick wake | threshold-based wake and normalized post-wake sample |
| LIS2DUX12 `MPU_INT` | motion/gesture/event where enabled | event classification; step counter is normally polled |
| IMU step counter mode | background activity count | deepest sleep class that preserves embedded function state |
| BLE/NINA | not an HW6 wake source by default | remains off/no wake unless future measured HW6 policy changes |

Unknown wake reasons are defects until explained.

HW6 FW0 controlled STOP2 START-wake evidence (`EV-HW6-20260814-P1-STOP2WAKE-056`) must be interpreted with debug state included. `DBGMCU->CR = 0x6` (`DBG_STOP | DBG_STANDBY`) caused an immediate return that correctly classified as `UNKNOWN`; clearing the real DBGMCU control register at `0xE0044004` to `0x0` produced the expected physical START wake classification with PA4 low and START edge evidence. Any future STOP2 wake/current run must record `DBGMCU->CR` before/after entry.
---

## Baseline State Sequence

1. Boot to an awake low-power runtime with display initialized and storage/audio idle.
2. Record expected wake mask and selected sleep class.
3. Request quiesce from every Platform owner and record acknowledgements.
4. Enter the selected sleep class.
5. Wake by RTC and confirm wake reason, RTC continuity, HAL tick/RTOS timebase recovery, and owner resume.
6. Repeat with `BTN_START`.
7. Repeat with A/B/L/R only for policies where those buttons are armed.
8. Repeat with `PMIC_INT` using a safe charger/input event.
9. Repeat with USB VBUS attach and confirm VBUS-only power does not enter MSC prompt/export; then validate USB data-host activity/enumeration separately before installer/storage policy can be offered.
10. Repeat with TMAG3001 threshold joystick wake once joystick threshold bring-up is complete.
11. Repeat with LIS2DUX12 event wake only for modes that intentionally arm IMU interrupt wake.
12. Validate IMU step-counter polling mode separately: no step interrupt wake required, but the sleep class must preserve embedded step counting.
13. Confirm the removed encoder and ambient-light paths cannot be armed or reported as HW6 wake sources.
14. Force one unknown or disabled wake path, if practical, and confirm it logs as a defect rather than silently routing to normal wake.

---

## Reactive Backend And Workload Measurement Matrix

Populate one row per tested physical waiting backend or active workload. Runtime class alone does not determine current.

| Runtime/workload | Physical backend | Armed wake sources | Average current | Wake latency | Resume latency | Failed wakes | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `SHELL` reactive wait | TBD | system/input policy | TBD | TBD | TBD | TBD | open |
| `LP_GRAPH` reactive hold wait | TBD | declared event/wake policy | TBD | TBD | TBD | TBD | open |
| `LP_MODULE` reactive waiting visual | TBD | declared event/wake policy | TBD | TBD | TBD | TBD | open |
| `RT_SCENE` active -> reactive fallback | active then TBD wait backend | declared activity/wake policy | TBD | TBD | TBD | TBD | open |
| `INSTALLER` waiting state | TBD | USB activity, B exit policy | TBD | TBD | TBD | TBD | open |
| IMU step counter active | TBD | RTC/poll cadence plus optional IMU policy | TBD | TBD | TBD | TBD | open |
| LPBAM waiting-visual backend | STOP2 + LPBAM | schedule/input/power/fault exit | TBD | TBD | TBD | TBD | open |

---

## Active Operating-Point Sweep Matrix

Populate one row per deterministic workload and candidate internal operating point. Keep the workload, peripheral state, build, assets, source voltage, and measurement setup identical across each comparison.

| Semantic/workload | Candidate point/config | Response or frame timing | Active duration | Average/peak current | Charge/energy | Deadline/failure result | Status |
|---|---|---|---|---|---|---|---|
| `REACTIVE` minimal event transaction | TBD | TBD | TBD | TBD | TBD | TBD | open |
| `REACTIVE` state change plus partial render | TBD | TBD | TBD | TBD | TBD | TBD | open |
| `REACTIVE` presentation plus waiting-visual preparation | TBD | TBD | TBD | TBD | TBD | TBD | open |
| `REALTIME` representative frame workload | TBD | TBD | sustained | TBD | TBD | TBD | open |
| `REALTIME` worst admitted frame plus audio/sensor contexts | TBD | TBD | sustained | TBD | TBD | TBD | open |
| operating-point transition and return to wait | source -> destination TBD | transition latency | TBD | TBD | TBD | break-even/failure TBD | open |

Reactive selection is based on complete event-to-yield transaction cost subject to latency/correctness limits. Realtime selection is based on the lowest sustained power that meets all required deadlines with margin. Follow [[Power_Measurement_and_Trace_Correlation_Runbook]] for capture windows and evidence.

---

## Failure Injection

At least these failure cases should be tested before sleep/wake policy is considered stable:

| Failure | Expected behavior |
| --- | --- |
| display flush active during sleep request | sleep waits for flush completion or times out/faults |
| audio DMA active during sleep request | audio drains/stops before sleep or blocks sleep |
| flash erase/program active during sleep request | storage blocks sleep until operation completes or faults |
| unknown wake source | logged as defect with raw wake evidence |
| PMIC read failure after wake | bounded recovery; no stale battery state silently trusted |
| clock/timebase restore failure | boot/fault supervisor path, not normal runtime |

---

## Validation Procedure

1. Validate owner quiesce acknowledgements before sleep entry.
2. Validate RTC wake and timebase recovery.
3. Validate Start wake.
4. Validate optional A/B/L/R wake if enabled by policy.
5. Validate PMIC interrupt wake/notification.
6. Validate joystick threshold wake if configured.
7. Validate IMU wake and step-counter sleep floor separately.
8. Validate that removed encoder/light wake intents are rejected by HW6 policy and target-profile validation.
9. Validate USB attach enters installer/transport policy.
10. Validate display remains visible and EXTCOMIN/VCOM policy remains correct.
11. Validate unknown wake reasons are logged as defects.
12. Measure current for each physical waiting backend and armed wake context where practical.
13. Validate active audio, storage, display, and sensor operations correctly block or delay sleep entry.
14. Validate IMU step-counter mode chooses the deepest sleep class that preserves embedded step counting, not the absolute deepest sleep class.
15. Sweep valid reactive operating points with identical deterministic event-to-yield workloads and select by transaction charge/energy subject to response limits.
16. Sweep valid realtime operating points with representative and worst admitted frame/audio/sensor workloads and select by deadline margin plus sustained power.
17. Measure operating-point transition latency/charge and establish a break-even interval before enabling dynamic switching or hysteresis.
18. Record pending selections in [[Pending_Measured_Constants_Register]] and promote them only with [[HW6_Brought_Up_Tracker]] evidence.

---

## Evidence Requirements

Record in [[HW6_Brought_Up_Tracker]]:

- sleep class tested
- wake source tested
- wake latency
- resume latency
- current measurement if available
- wake reason classification
- failed wake/resume count
- display hold observation

Do not claim an HW6 wake source is supported until measured on an identified HW6 board.
