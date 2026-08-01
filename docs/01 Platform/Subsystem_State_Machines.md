# Subsystem State Machines (Required)

Every major subsystem must have an explicit finite state machine (FSM) defined before implementation.

This document is the required baseline model for this project. A target instantiates only the FSMs for capabilities present in its published target profile. The active HW6 target does not instantiate the retired light-sensor, rotary-encoder, or BBB audio paths.

---

## FSM Contract Rules

- Each subsystem must expose one state enum.
- Transitions must occur through explicit events.
- Every transition must define entry/exit actions.
- Error and recovery paths must be explicit.
- No state transitions from ISR context.

Required implementation artifacts per subsystem:
1. `*_state.h` enum definitions
2. `*_events.h` event enum definitions
3. transition table in source
4. trace hook for state transitions (rate-limited)

---

## HW6 FW0 Owner-Lifecycle Baseline

The first HW6 implementation of the retained-peripheral state machines is built
in `firmware/peepshow_hw6_fw0` and its one-way inactive lifecycle baseline has
passed on `HW6-UNIT-001`. This section records the accepted stabilization slice,
not the complete production transition model.

- Public state and event contracts are split by subsystem in
  `Core/Inc/ps_{power,display,audio,input,sensor,storage,comm}_{state,events}.h`.
- `Core/Src/ps_hw6_owner_state_machines.c` contains explicit transition tables,
  invalid-transition rejection, per-machine status, and a bounded 128-entry
  transition trace.
- The seven physical ThreadX owners retain exclusive hardware access. `thPower`
  coordinates the baseline through the existing queues and bounded event-group
  acknowledgements; it does not operate another owner's peripheral directly.
- A lifecycle pass is never run automatically at boot. It is armed explicitly
  with `__fw0_owner_sm_start.gdb`, allowing normal RTOS startup to remain free of
  display, audio, sensor-register, flash-power, or NINA-mode side effects.
- The baseline target states are `PWR_ACTIVE_LP`, `PMIC_MONITOR`,
  `DISP_STATIC_HOLD`, `AUDIO_IDLE`, `SPK_OFF`, `JOY_SUSPENDED`,
  `IMU_SUSPENDED`, `STORAGE_FLASH_READY`, `FLASH_DEEP_POWER_DOWN`, and
  `BLE_SUSPENDED`.
- On 2026-07-31 lifecycle v2 passed with required/completed masks `0x7F/0x7F`,
  success/failure masks `0x7F/0x00`, and no rejected transitions. Evidence is
  preserved in `EV-HW6-20260731-P5-OWNERS-004`.
- The first lifecycle-v3 board run on 2026-08-01 completed every queue
  send/wait/ack and passed every physical owner except the LIS2DUX12 path.
  Its first post-deep-power-down I2C transaction returned `HAL_ERROR` exactly
  where ST documents the wake NACK, but v3 did not retain the raw AF error and
  treated the response as a fault, leaving the IMU FSM in `IMU_ERROR`. The
  failed diagnostic is preserved in
  `EV-HW6-20260801-P5-OWNERS-005`.
- Lifecycle v5 passed the same two bounded owner-routed
  `inactive -> active -> inactive` cycles without STOP2 on `HW6-UNIT-001`.
  Its IMU path uses the official LIS2DUX12 driver and I2C-specific wake
  contract: require the first address NACK with `HAL_I2C_ERROR_AF`, wait 30 ms,
  then require ACK and `WHO_AM_I=0x47` before configuring active low-rate mode
  with `CTRL5=0x10`. Evidence is preserved in
  `EV-HW6-20260801-P5-OWNERS-006`.
- Lifecycle v6 kept the same owner cycle and moved the TMAG3001 input path out
  of owner-local raw register code into the HW6-native `ps_dev_tmag3001` wrapper
  under the I2C3 input lease. It passed identity, expected sleeping-device wake
  status `5` followed by retry success, active `SENSOR_CONFIG1=0x70` /
  `DEVICE_CONFIG2=0x02`, and terminal sleep recommit. Evidence is preserved in
  `EV-HW6-20260801-P5-INPUT-007`.
- The lifecycle-v5 active boundary is `PWR_ACTIVE_RT`, `PMIC_MONITOR`,
  `DISP_STATIC_HOLD`, `AUDIO_IDLE`, `SPK_OFF`, `JOY_SLOW_POLL`,
  `IMU_LOW_RATE_SAMPLE`, `STORAGE_FLASH_READY`, `FLASH_READY`, and `BLE_IDLE`.
  Display and audio return to their stable ready states after completing their
  finite diagnostic action; they are not held busy merely to satisfy a probe.
- The joystick/TMAG3001 and LIS2DUX12 paths use read-modify-write plus exact
  readback verification while their register maps remain accessible. Their
  sleep/deep-power-down writes are terminal: no post-write address transaction
  is allowed because that would wake the device. Flash identity is checked
  before deep power-down. The NINA path verifies every supported AT response
  before using `AT&D4`, deasserting the module's `NINA_DSR` host-DTR control
  input on `PC8`, waiting for STOP entry, and deinitializing its UART.
  `PC7/NINA_DTR` remains a module-status output and is not driven.
- USB device mode remains part of the storage-owner state machine. For the
  detached baseline, `thStorage` requires VBUS low, deinitializes the PCD, and
  verifies that both the USB clock and VDDUSB domain are disabled. A connected
  host is rejected by this baseline rather than silently powered down.
- Button, classifier, and START-overlay contracts are compiled, but physical
  button transitions remain deferred until the HW6 housing makes the inputs
  accessible.

This is a stabilization slice, not the complete production transition model.
Lifecycle-v6 keeps the bounded owner-routed repeatability check passing with both LIS2DUX12 and TMAG3001 driver-backed paths, but
cancellation, injected faults, retries, STOP2 handoff, saturation, current, and
production runtime mode changes remain separate required evidence.

---

## 1) System Performance State Machine

States:
- `SYS_PERF_DEEP_IDLE`
- `SYS_PERF_LP_ACTIVE`
- `SYS_PERF_RT_LOW`
- `SYS_PERF_RT_MED`
- `SYS_PERF_RT_HIGH`
- `SYS_USB_INSTALL`

Key events:
- `EV_MODE_SET`
- `EV_INPUT_ACTIVITY`
- `EV_AUDIO_ACTIVE`
- `EV_AUDIO_IDLE`
- `EV_INSTALL_ENTER`
- `EV_INSTALL_EXIT`
- `EV_INACTIVITY_TIMEOUT`

---

## 2) Sleep Class State Machine

States:
- `SLEEP_NONE`
- `SLEEP_WFI_IDLE`
- `SLEEP_STOP2`
- `SLEEP_STOP3`
- `SLEEP_STANDBY`

Key events:
- `EV_SLEEP_ALLOWED`
- `EV_SLEEP_BLOCKED`
- `EV_WAKE_REASON_<...>`
- `EV_DEADLINE_NEAR`

Owner: power thread only.

---

## 3) System Power State Machine

Detailed contract: [[PMIC_and_Power_Contract]]

States:
- `PWR_BOOTING`
- `PWR_RAIL_VALIDATE`
- `PWR_ACTIVE_LP`
- `PWR_ACTIVE_RT`
- `PWR_SLEEP_PREP`
- `PWR_STOP_RESIDENT`
- `PWR_WAKE_RESUME`
- `PWR_FORCED_SLEEP`
- `PWR_SHIP_PREP`
- `PWR_FAULT`

Rules:
- `thPower` is the only owner of this state machine.
- Low battery threshold enters `PWR_FORCED_SLEEP`.
- Critical battery threshold disconnects the ISOFET.
- START shipping-mode prep is separate from low/critical battery handling.

---

## 4) ADP5360 / Battery State Machine

Detailed contract: [[PMIC_and_Power_Contract]]

States:
- `PMIC_OFFLINE`
- `PMIC_PROBE`
- `PMIC_CONFIG`
- `PMIC_MONITOR`
- `PMIC_CHARGING`
- `PMIC_CHARGE_DONE`
- `PMIC_LOW_BATT`
- `PMIC_CRITICAL_BATT`
- `PMIC_SHIP_PENDING`
- `PMIC_RECOVERING`
- `PMIC_ERROR`

Rules:
- ADP5360 is reached over I2C at address `0x46`.
- `PMIC_INT` on `PB15` notifies or wakes, but state handling occurs in `thPower`.
- VBUS can be classified through ADP5360 and `USB_OTG_FS_VBUS` on `PA9`.
- Device may run while charging except while in flashing/install mode.

---

## 5) Display State Machine
Detailed contract: [[Display_and_Rendering_Contract]]

Display owner states:
- `DISP_OFF`
- `DISP_INIT`
- `DISP_STATIC_HOLD`
- `DISP_LP_DIRTY`
- `DISP_LP_FLUSH`
- `DISP_RT_ACTIVE`
- `DISP_RT_FLUSH`
- `DISP_LPBAM_ARM`
- `DISP_LPBAM_RUNNING`
- `DISP_LPBAM_EXIT`
- `DISP_SUSPENDING`
- `DISP_SUSPENDED`
- `DISP_RECOVERING`
- `DISP_ERROR`

Flush states:
- `FLUSH_IDLE`
- `FLUSH_BUILD_CMD`
- `FLUSH_TRANSFER_ACTIVE`
- `FLUSH_WAIT_COMPLETE`
- `FLUSH_COMMIT`
- `FLUSH_ABORT`
- `FLUSH_ERROR`

LPBAM display states:
- `LPBAM_DISP_UNAVAILABLE`
- `LPBAM_DISP_PREPARE`
- `LPBAM_DISP_VALIDATE_LIST`
- `LPBAM_DISP_ARMED`
- `LPBAM_DISP_RUNNING`
- `LPBAM_DISP_WAKE_REQUESTED`
- `LPBAM_DISP_STOPPING`
- `LPBAM_DISP_FAULT`

Key events:
- `EV_DISP_ENABLE_REQ`
- `EV_DISP_INIT_OK`
- `EV_DISP_INVALIDATE`
- `EV_DISP_PRESENT`
- `EV_DISP_FLUSH_DONE`
- `EV_DISP_STATIC_HOLD_REQ`
- `EV_DISP_RT_ENTER`
- `EV_DISP_LP_ENTER`
- `EV_DISP_LPBAM_REQ`
- `EV_DISP_LPBAM_RUNNING`
- `EV_DISP_LPBAM_WAKE_REQ`
- `EV_QUIESCE`
- `EV_RESUME`
- `EV_DISP_FAULT`
- `EV_DISP_RECOVER_OK`

Rules:
- `thDisplay` owns SPI3, LPDMA, framebuffer flushes, and dirty tracking. HW6 has no software-controlled translator OE; its display translator is hardwired enabled.
- `LCD_1HZ` / EXTCOMIN is maintained from RTC while the display holds an image.
- Logical display resolution is landscape `168 x 144`; native panel mapping is portrait `144 x 168`.
- Present in `DISP_SUSPENDED` is invalid and must be rejected.
- LPBAM display animation is allowed only for prevalidated low-power idle sequences.

---

## 6) Button Input State Machine

Detailed contract: [[Button_Input_Contract]]

Per-button states:
- `BTN_DISABLED`
- `BTN_RELEASED`
- `BTN_DEBOUNCE_PRESS`
- `BTN_PRESSED`
- `BTN_HELD`
- `BTN_REPEAT`
- `BTN_DEBOUNCE_RELEASE`
- `BTN_STUCK`
- `BTN_ERROR`

Classifier states:
- `BTN_CLASSIFIER_IDLE`
- `BTN_CLASSIFIER_SINGLE_PENDING`
- `BTN_CLASSIFIER_CHORD_PENDING`
- `BTN_CLASSIFIER_CHORD_ACTIVE`
- `BTN_CLASSIFIER_REPEAT_ACTIVE`
- `BTN_CLASSIFIER_LOCKED`

Start shipping overlay states:
- `START_IDLE`
- `START_NORMAL_PRESS`
- `START_LONG_PRESS`
- `START_SHIP_PREP`
- `START_SHIP_WARNING`
- `START_SHIP_IMMINENT`
- `START_RELEASED`

Key events:
- `EV_BTN_EDGE_ACTIVE`
- `EV_BTN_EDGE_INACTIVE`
- `EV_BTN_DEBOUNCE_DONE`
- `EV_BTN_LONG_THRESHOLD`
- `EV_BTN_REPEAT_THRESHOLD`
- `EV_BTN_REPEAT_TICK`
- `EV_BTN_STUCK_THRESHOLD`
- `EV_START_SHIP_PREP_THRESHOLD`
- `EV_START_SHIP_WARN_THRESHOLD`
- `EV_START_SHIP_IMMINENT_THRESHOLD`

Rules:
- Raw edges from ISR only enqueue input events.
- `thInput` owns debounce, repeats, and chord classification.
- Platform emits raw logical button events and chord masks; UI/Engine/Game map them contextually.
- `BTN_BOOT` is hardware BOOT0; firmware cannot intercept ROM bootloader entry at reset, and application-visible `BTN_BOOT` is maintenance-only, not Engine/Game input.
- `BTN_START` shipping-mode intent must be routed to power/save policy before the ADP5360 hardware threshold.

---

## 7) Joystick/Hall State Machine

Detailed contract: [[Joystick_Hall_Input_Contract]]

States:
- `JOY_OFF`
- `JOY_PROBE`
- `JOY_CONFIG`
- `JOY_CAL_REQUIRED`
- `JOY_CENTER_CAL`
- `JOY_THRESHOLD_ARMED`
- `JOY_WAKE_PENDING`
- `JOY_DIRECTION_SAMPLE`
- `JOY_SLOW_POLL`
- `JOY_FAST_POLL`
- `JOY_NORMALIZE`
- `JOY_SAFE_MODE`
- `JOY_SUSPENDED`
- `JOY_RECOVERING`
- `JOY_ERROR`

Key events:
- `EV_JOY_ENABLE_REQ`
- `EV_JOY_DISABLE_REQ`
- `EV_PROBE_OK`
- `EV_CONFIG_OK`
- `EV_CAL_MISSING`
- `EV_CAL_START`
- `EV_CAL_DONE`
- `EV_JOY_THRESHOLD_ARM_REQ`
- `EV_JOY_INT`
- `EV_JOY_SAMPLE_DONE`
- `EV_JOY_NORMALIZE_DONE`
- `EV_SLOW_POLL_REQ`
- `EV_FAST_POLL_REQ`
- `EV_QUIESCE`
- `EV_RESUME`
- `EV_I2C_ERROR`
- `EV_RECOVER_OK`

Rules:
- `thInput` owns TMAG3001 joystick policy and normalized event publication.
- Public joystick data is normalized only; raw magnetic data is diagnostic/calibration only.
- `JOY_INT` schedules a bounded read before direction is published.
- Missing calibration routes to safe-mode calibration.
- Joystick failure routes to safe mode with available button fallback navigation. On HW6 this is the L/R/START button path; no rotary fallback exists.

---

## 8) IMU State Machine

Detailed contract: [[IMU_Contract]]

States:
- `IMU_OFF`
- `IMU_PROBE`
- `IMU_CONFIG`
- `IMU_EMBEDDED_FUNC_ENABLE`
- `IMU_STEP_COUNTER_ACTIVE`
- `IMU_EVENT_ARMED`
- `IMU_LOW_RATE_SAMPLE`
- `IMU_STREAMING`
- `IMU_SUSPENDED`
- `IMU_RECOVERING`
- `IMU_ERROR`

Key events:
- `EV_MODE_CHANGE`
- `EV_PROBE_OK`
- `EV_CONFIG_OK`
- `EV_EMBEDDED_ENABLE_OK`
- `EV_STEP_COUNTER_REQUEST`
- `EV_WAKE_HINT_STEP`
- `EV_WAKE_HINT_MOTION`
- `EV_WAKE_HINT_GESTURE`
- `EV_LOW_RATE_SAMPLE_REQUEST`
- `EV_STREAM_REQUEST`
- `EV_STREAM_STOP`
- `EV_I2C_ERROR`
- `EV_RECOVER_OK`

Rules:
- Package/runtime requests intent, not sensor register writes.
- `IMU_STEP_COUNTER_ACTIVE` publishes a power floor to preserve embedded smart functions.
- IMU failure is non-critical and recoverable.

---

## 9) Audio State Machine

Detailed contract: [[Audio_Contract]]

Owner states:
- `AUDIO_OFF`
- `AUDIO_INIT`
- `AUDIO_IDLE`
- `AUDIO_ACTIVE`
- `AUDIO_SUSPENDING`
- `AUDIO_SUSPENDED`
- `AUDIO_RECOVERING`
- `AUDIO_ERROR`

Speaker states:
- `SPK_OFF`
- `SPK_ENABLE`
- `SPK_IDLE`
- `SPK_PRELOAD`
- `SPK_BUFFERING`
- `SPK_PLAYING`
- `SPK_DRAINING`
- `SPK_PAUSED`
- `SPK_UNDERRUN`
- `SPK_ERROR`

Legacy/capability-gated BBB states (not instantiated on HW6):
- `BBB_OFF`
- `BBB_READY`
- `BBB_PATTERN_LOAD`
- `BBB_PLAYING`
- `BBB_DRAINING`
- `BBB_ERROR`

Key events:
- `EV_AUDIO_CMD_UI_SFX`
- `EV_AUDIO_CMD_MUSIC_START`
- `EV_AUDIO_CMD_MUSIC_STOP`
- `EV_AUDIO_CMD_STREAM_START`
- `EV_AUDIO_CMD_STREAM_STOP`
- `EV_AUDIO_CMD_BBB_START`
- `EV_AUDIO_CMD_BBB_STOP`
- `EV_QUIESCE`
- `EV_RESUME`
- `EV_DMA_ERROR`
- `EV_AUDIO_UNDERRUN`
- `EV_AUDIO_RECOVER_OK`
- `EV_RECOVER_OK`

Rules:
- `thAudio` owns every audio path present on the active target. HW6 exposes the speaker path only.
- Speaker output supports exactly 1 music voice plus 5 SFX voices mixed to 16 kHz mono PCM.
- On legacy targets granting `audio.bbb`, BBB output is procedural symbolic audio through `BUZZ`, not PCM streaming.
- Speaker/BBB concurrency applies only to targets that physically provide and grant both paths; it is not an HW6 behavior.
- No blocking refill operations.
- Owner thread publishes activity status to power manager.

---

## 10) Legacy Light Sensor State Machine (Absent On HW6)

Detailed contract: [[Light_Sensor_Contract]]

States:
- `LIGHT_OFF`
- `LIGHT_SETTLING`
- `LIGHT_SINGLE_SAMPLE`
- `LIGHT_PERIODIC_WAIT`
- `LIGHT_STREAMING`
- `LIGHT_PROCESSING`
- `LIGHT_PUBLISH`
- `LIGHT_SUSPENDED`
- `LIGHT_ERROR`

Key events:
- `EV_LIGHT_ONESHOT_REQ`
- `EV_LIGHT_PERIODIC_REQ`
- `EV_LIGHT_STREAM_REQ`
- `EV_LIGHT_STOP_REQ`
- `EV_LIGHT_SETTLE_DONE`
- `EV_LIGHT_PERIOD_ELAPSED`
- `EV_LIGHT_ADC_DONE`
- `EV_LIGHT_PROCESS_DONE`
- `EV_LIGHT_PUBLISH_DONE`
- `EV_LIGHT_NO_CHANGE`
- `EV_LIGHT_STREAM_EXPIRED`
- `EV_QUIESCE`
- `EV_RESUME`
- `EV_LIGHT_FAULT`
- `EV_RECOVER_OK`

These states and events are retained for retired HW5 evidence and any future target that grants `sensor.light`. They are not compiled or instantiated for HW6.

Rules for a target that grants the capability:
- `thSensor` owns `PHOT_EN`, `ADC1`, and ambient-light sampling.
- The sensor supports one-shot, low-rate periodic, and bounded streaming modes.
- Normal Engine/Reference Game output is normalized `0-100` plus light band; raw ADC is diagnostic only.
- Platform may clamp or reject requested streaming rates and durations.

---

## 11) Legacy Rotary Encoder State Machine (Absent On HW6)

Detailed contract: [[Rotary_Encoder_Input_Contract]]

States:
- `ENC_OFF`
- `ENC_POWER_SETTLE`
- `ENC_IDLE`
- `ENC_TRACKING`
- `ENC_ACCEL`
- `ENC_WAKE_ARMED`
- `ENC_SUSPENDED`
- `ENC_ERROR`

Key events:
- `EV_ENC_ENABLE_REQ`
- `EV_ENC_DISABLE_REQ`
- `EV_ENC_WAKE_ARM_REQ`
- `EV_ENC_WAKE_DISARM_REQ`
- `EV_ENC_SETTLE_EXPIRED`
- `EV_ENC_COUNTER_DELTA`
- `EV_ENC_ACCEL_DETECTED`
- `EV_ENC_ACTIVITY_TIMEOUT`
- `EV_QUIESCE`
- `EV_RESUME`
- `EV_ENC_FAULT`
- `EV_RECOVER_OK`

These states and events are retained for retired HW5 evidence and any future target that grants `input.encoder`. They are not compiled or instantiated for HW6.

Rules for a target that grants the capability:
- `thInput` owns `TIM2` encoder mode and `ENC_EN`.
- Logical encoder delta events are emitted only after counter deltas are classified.
- `ENC_WAKE_ARMED` keeps the powered encoder path active only under explicit Platform policy.
- No stale counter delta may be emitted after quiesce/resume.

---

## 12) BLE Communication State Machine

Detailed contract: [[BLE_Communication_Contract]]

States:
- `BLE_OFF`
- `BLE_RESET_ASSERT`
- `BLE_BOOT_WAIT`
- `BLE_CONFIG`
- `BLE_IDLE`
- `BLE_ADVERTISING`
- `BLE_CONNECTED`
- `BLE_PAIRING`
- `BLE_TRANSFER`
- `BLE_SUSPENDING`
- `BLE_SUSPENDED`
- `BLE_ERROR`

Key events:
- `EV_BLE_ENABLE_REQ`
- `EV_BLE_DISABLE_REQ`
- `EV_BLE_RESET_ASSERTED`
- `EV_BLE_BOOT_READY`
- `EV_BLE_BOOT_TIMEOUT`
- `EV_BLE_CONFIG_OK`
- `EV_BLE_CONFIG_FAIL`
- `EV_BLE_ADV_START_REQ`
- `EV_BLE_ADV_STOP_REQ`
- `EV_BLE_CONNECTED`
- `EV_BLE_DISCONNECTED`
- `EV_BLE_PAIRING_START`
- `EV_BLE_PAIRING_DONE`
- `EV_BLE_BOND_STORE_OK`
- `EV_BLE_BOND_STORE_FAIL`
- `EV_BLE_TX_REQ`
- `EV_BLE_RX_READY`
- `EV_BLE_TRANSFER_DONE`
- `EV_QUIESCE`
- `EV_RESUME`
- `EV_BLE_FAULT`
- `EV_RECOVER_OK`
- `EV_RECOVER_EXHAUSTED`

Rules:
- `thComm` owns `LPUART1`, NINA reset/mode pins, BLE command framing, and BLE recovery.
- Engine-facing APIs expose generic communication capabilities, not module internals.
- All command/response, pairing, bonding, and transfer paths must be bounded.
- BLE defaults off unless multiplayer, companion, pairing, or diagnostics explicitly requests it.
- BLE wake is not granted on HW6 unless later measured evidence and the HW6 target profile explicitly enable it.

---

## 13) Storage Ownership State Machine

Detailed contract: [[Storage_and_Installer_Contract]]

States:
- `STORAGE_OFFLINE`
- `STORAGE_INIT`
- `STORAGE_FLASH_READY`
- `STORAGE_LOCAL_MOUNT`
- `STORAGE_LOCAL_READY`
- `STORAGE_QUIESCE_LOCAL`
- `STORAGE_PREPARE_USB`
- `STORAGE_USB_STAGING_EXPORTED`
- `STORAGE_USB_STAGING_DIRTY`
- `STORAGE_USB_RELEASE`
- `STORAGE_INSTALLING`
- `STORAGE_RECOVERING`
- `STORAGE_SAFE_MODE`
- `STORAGE_ERROR`

Key events:
- `EV_STORAGE_INIT`
- `EV_FLASH_READY`
- `EV_MOUNT_OK`
- `EV_MOUNT_FAIL`
- `EV_LOCAL_QUIESCE_REQ`
- `EV_LOCAL_QUIESCE_OK`
- `EV_INSTALLER_ENTER`
- `EV_UMOUNT_OK`
- `EV_USB_VBUS_PRESENT`
- `EV_USB_ACTIVITY_DETECTED`
- `EV_USB_ENUMERATED`
- `EV_USB_MSC_ENTRY_ACCEPTED`
- `EV_USB_DETACH`
- `EV_USB_HOST_DIRTY`
- `EV_USB_RELEASE_REQ`
- `EV_USB_RESCAN_OK`
- `EV_INSTALL_START`
- `EV_INSTALL_DONE`
- `EV_STORAGE_FAULT`
- `EV_RECOVER_OK`

Rules:
- Only `thStorage` may mount/unmount/export.
- VBUS presence alone is not an MSC entry event.
- USB protocol activity or host enumeration must gate MSC availability before installer/export entry is accepted.
- USB MSC exposes only the staging/export region.
- Runtime/package access must be blocked in `STORAGE_USB_STAGING_EXPORTED`.
- Storage failure routes to safe mode because settings/calibration are required for normal usability.

---

## 14) External Flash Device State Machine

States:
- `FLASH_OFF`
- `FLASH_RESET`
- `FLASH_PROBE`
- `FLASH_CONFIG`
- `FLASH_READY`
- `FLASH_BUSY_READ`
- `FLASH_BUSY_PROGRAM`
- `FLASH_BUSY_ERASE`
- `FLASH_DEEP_POWER_DOWN`
- `FLASH_RECOVERING`
- `FLASH_ERROR`

Key events:
- `EV_BOOT`
- `EV_PROBE_OK`
- `EV_PROBE_FAIL`
- `EV_CONFIG_OK`
- `EV_REQ_READ`
- `EV_REQ_WRITE`
- `EV_REQ_ERASE`
- `EV_REQ_DEEP_POWER_DOWN`
- `EV_WAKE_REVALIDATE`
- `EV_OP_DONE`
- `EV_FLASH_FAULT`
- `EV_RECOVER_RETRY`

Rules:
- External flash is `AT25SL128A` on `OCTOSPI1`.
- Recovery retries must be bounded and timed.
- Flash should enter deep power-down whenever idle and policy allows.

---

## 15) Runtime Manager State Machine

States:
- `RUNTIME_NONE`
- `RUNTIME_MOUNTING`
- `RUNTIME_RUNNING`
- `RUNTIME_SUSPENDING`
- `RUNTIME_SUSPENDED`
- `RUNTIME_RESUMING`
- `RUNTIME_UNMOUNTING`
- `RUNTIME_ERROR`

Key events:
- `EV_RUNTIME_SELECT`
- `EV_MOUNT_OK`
- `EV_START_OK`
- `EV_SUSPEND_REQ`
- `EV_SUSPEND_OK`
- `EV_RESUME_REQ`
- `EV_RESUME_OK`
- `EV_UNMOUNT_REQ`
- `EV_UNMOUNT_OK`
- `EV_RUNTIME_FAULT`

Rules:
- Runtime switch is illegal unless current runtime reaches `RUNTIME_NONE` or `RUNTIME_SUSPENDED`.

---

## 16) Installer Workflow State Machine

States:
- `INSTALL_IDLE`
- `INSTALL_STAGE`
- `INSTALL_VALIDATE`
- `INSTALL_COMMIT`
- `INSTALL_ROLLBACK`
- `INSTALL_DONE`
- `INSTALL_ERROR`

Key events:
- `EV_PACKAGE_DETECTED`
- `EV_STAGE_OK`
- `EV_VALIDATE_OK`
- `EV_VALIDATE_FAIL`
- `EV_COMMIT_OK`
- `EV_COMMIT_FAIL`
- `EV_ROLLBACK_OK`

Rules:
- Commit must be atomic from package-manager perspective.
- Rollback path must be tested and documented.

---

## 17) Boot and Fault Supervisor State Machines

Detailed contract: [[Boot_and_Fault_Supervisor_State_Machine]]

Boot states:
- `BOOT_RESET_ENTRY`
- `BOOT_SAFE_GPIO`
- `BOOT_CLOCKS`
- `BOOT_KERNEL_START`
- `BOOT_PLATFORM_PROBE`
- `BOOT_STORAGE_LOAD`
- `BOOT_RECOVERY_CHECK`
- `BOOT_SAFE_MODE`
- `BOOT_RUNTIME_READY`
- `BOOT_FAULT_LATCHED`

Fault states:
- `FAULT_HEALTHY`
- `FAULT_CAPTURE`
- `FAULT_CLASSIFY`
- `FAULT_RECOVERING`
- `FAULT_DEGRADED`
- `FAULT_SAFE_MODE`
- `FAULT_FATAL`

Key events:
- `EV_RESET_VECTOR`
- `EV_SAFE_GPIO_DONE`
- `EV_CLOCKS_OK`
- `EV_CLOCKS_FAIL`
- `EV_KERNEL_OK`
- `EV_KERNEL_FAIL`
- `EV_PLATFORM_REQUIRED_OK`
- `EV_PLATFORM_REQUIRED_FAIL`
- `EV_STORAGE_READY`
- `EV_STORAGE_MISSING_SETTINGS`
- `EV_CAL_MISSING`
- `EV_FIRST_BOOT_DETECTED`
- `EV_RECOVERY_REQUIRED`
- `EV_RUNTIME_ALLOWED`
- `EV_FATAL_BOOT_FAULT`
- `EV_FAULT_RAISED`
- `EV_FAULT_CAPTURED`
- `EV_FAULT_CLASSIFIED_DEGRADED`
- `EV_FAULT_CLASSIFIED_RECOVERABLE`
- `EV_FAULT_CLASSIFIED_SAFE_MODE`
- `EV_FAULT_CLASSIFIED_FATAL`
- `EV_RECOVERY_EXHAUSTED`
- `EV_FAULT_CLEARED`

Rules:
- Normal shell/runtime may not start until required settings and calibration are valid.
- Missing settings or missing joystick calibration routes to safe-mode setup/calibration.
- `BTN_BOOT` is hardware BOOT0; firmware cannot intercept ROM bootloader entry when sampled high at reset.
- First boot ignores START shipping intent because no user data exists yet.
- Display boot failure is fatal for normal standalone operation and must produce the easiest available diagnostic evidence.
- Fault evidence should be persisted to protected external flash through `thStorage` once storage is available.

---

## Transition Logging Policy

Each subsystem must emit:
- state enter event ID
- state exit event ID
- fault transition event ID

Logging must be non-blocking and rate-limited.

---

## Minimum Acceptance For Any FSM

1. Full transition table documented.
2. Invalid transitions are rejected with explicit error.
3. Recovery path tested.
4. Transition logs can reconstruct runtime sequence.

---

## Extended FSM Coverage

Detailed state-machine contracts that complement this baseline:
- [[Shell_and_UI_Navigation_State_Machine]]
- [[Package_Manager_State_Machine]]
- [[Boot_and_Fault_Supervisor_State_Machine]]
- [[Runtime_Host_Internal_State_Machines]]
