# Subsystem State Machines (Required)

Every major subsystem must have an explicit finite state machine (FSM) defined before implementation.

This document is the required baseline model for this project.

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

## 3) Display State Machine

States:
- `DISP_OFF`
- `DISP_STATIC_HOLD`
- `DISP_LP_UPDATE`
- `DISP_RT_UPDATE`
- `DISP_SUSPENDED`
- `DISP_ERROR`

Key events:
- `EV_DISP_INIT_OK`
- `EV_DISP_INVALIDATE`
- `EV_DISP_PRESENT`
- `EV_MODE_CHANGE`
- `EV_QUIESCE`
- `EV_RESUME`
- `EV_DISP_FAULT`
- `EV_DISP_RECOVER_OK`

Rules:
- Display owner thread controls all state transitions.
- Present in `DISP_SUSPENDED` is invalid and must be rejected.

---

## 4) Button Input State Machine

States:
- `BTN_IDLE`
- `BTN_DEBOUNCE_PRESS`
- `BTN_PRESSED`
- `BTN_DEBOUNCE_RELEASE`

Key events:
- `EV_GPIO_EDGE`
- `EV_DEBOUNCE_EXPIRED`
- `EV_REPEAT_TICK`

Rules:
- Raw edges from ISR only enqueue input events.
- Logical actions generated in input owner thread.

---

## 5) Joystick/Hall State Machine

States:
- `JOY_OFF`
- `JOY_THRESHOLD_IRQ`
- `JOY_SLOW_POLL`
- `JOY_FAST_POLL`
- `JOY_CALIBRATION`
- `JOY_ERROR`

Key events:
- `EV_MODE_CHANGE`
- `EV_ACTIVITY_HIGH`
- `EV_ACTIVITY_LOW`
- `EV_CAL_START`
- `EV_CAL_DONE`
- `EV_I2C_ERROR`
- `EV_RECOVER_OK`

Rules:
- Power policy may request state changes; input owner applies them.
- Poll rates must be compile-time knobs.

---

## 6) IMU State Machine

States:
- `IMU_OFF`
- `IMU_STEP_ONLY`
- `IMU_EVENT_IRQ`
- `IMU_ACTIVE_STREAM`
- `IMU_ERROR`

Key events:
- `EV_MODE_CHANGE`
- `EV_WAKE_HINT_STEP`
- `EV_WAKE_HINT_MOTION`
- `EV_STREAM_REQUEST`
- `EV_STREAM_STOP`
- `EV_I2C_ERROR`
- `EV_RECOVER_OK`

Rules:
- Package/runtime requests intent, not sensor register writes.

---

## 7) Audio State Machine

States:
- `AUDIO_OFF`
- `AUDIO_UI_ONLY`
- `AUDIO_STREAMING`
- `AUDIO_SUSPENDED`
- `AUDIO_ERROR`

Key events:
- `EV_AUDIO_CMD_UI_SFX`
- `EV_AUDIO_CMD_STREAM_START`
- `EV_AUDIO_CMD_STREAM_STOP`
- `EV_QUIESCE`
- `EV_RESUME`
- `EV_DMA_ERROR`
- `EV_RECOVER_OK`

Rules:
- No blocking refill operations.
- Owner thread publishes activity status to power manager.

---

## 8) Storage Ownership State Machine

States:
- `STORAGE_LOCAL`
- `STORAGE_PREPARE_USB`
- `STORAGE_USB_EXPORTED`
- `STORAGE_INSTALLING`
- `STORAGE_RECOVERING`
- `STORAGE_ERROR`

Key events:
- `EV_INSTALLER_ENTER`
- `EV_UMOUNT_OK`
- `EV_USB_ATTACH`
- `EV_USB_DETACH`
- `EV_INSTALL_START`
- `EV_INSTALL_DONE`
- `EV_MOUNT_OK`
- `EV_STORAGE_FAULT`
- `EV_RECOVER_OK`

Rules:
- Only storage owner may mount/unmount/export.
- Runtime/package access must be blocked in `STORAGE_USB_EXPORTED`.

---

## 9) External Flash Device State Machine

States:
- `FLASH_OFFLINE`
- `FLASH_INIT`
- `FLASH_ONLINE`
- `FLASH_BUSY`
- `FLASH_ERROR`

Key events:
- `EV_BOOT`
- `EV_PROBE_OK`
- `EV_REQ_READ`
- `EV_REQ_WRITE`
- `EV_OP_DONE`
- `EV_FLASH_FAULT`
- `EV_RECOVER_RETRY`

Rules:
- Recovery retries must be bounded and timed.

---

## 10) Runtime Manager State Machine

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

## 11) Installer Workflow State Machine

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

## 12) Public Knob Service State Machine

States:
- `PUBKNOB_IDLE`
- `PUBKNOB_VALIDATE`
- `PUBKNOB_ROUTE_OWNER`
- `PUBKNOB_WAIT_OWNER`
- `PUBKNOB_APPLIED`
- `PUBKNOB_REJECTED`
- `PUBKNOB_ERROR`

Key events:
- `EV_PUBKNOB_REQUEST`
- `EV_VALIDATE_OK`
- `EV_VALIDATE_FAIL`
- `EV_OWNER_ROUTE_OK`
- `EV_OWNER_ROUTE_FAIL`
- `EV_OWNER_APPLY_OK`
- `EV_OWNER_APPLY_FAIL`
- `EV_REPLY_SENT`

Rules:
- Only knobs marked `public` are accepted.
- Validation must enforce mode and bounds before owner routing.
- Owner thread is the only entity allowed to apply value changes.
- Rejected requests must return explicit error reason.

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
- `27_shell_ui_navigation_state_machine.md`
- `28_package_manager_state_machine.md`
- `29_boot_and_fault_supervisor_state_machine.md`
- `30_runtime_host_internal_state_machines.md`
