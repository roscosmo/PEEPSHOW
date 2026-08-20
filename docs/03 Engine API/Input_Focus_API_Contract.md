# Input Focus API Contract

This document defines the Engine-facing input and focus model used by packages, tools, runtime hosts, and the digital twin.

It consumes Platform logical input events and normalized input snapshots. It does not expose GPIO pins, EXTI lines, timer counters, I2C registers, raw magnetic readings, debounce state, calibration records, wake-pin configuration, or sleep policy.

---

## Boundary

Platform owns:

- raw button capture, debounce, holds, repeats, and chord masks
- Start shipping/power-intent overlay
- `BTN_BOOT` maintenance/recovery policy
- rotary encoder power, timer, wake-arm, and delta classification
- joystick I2C ownership, calibration, threshold/wake policy, and normalization
- input fault detection and safe-mode input fallback
- wake-source arming and sleep compatibility

Engine owns:

- focus scopes
- package action names
- input maps
- logical bindings
- action state
- repeat/chord/hold policy at the package-facing layer
- scene input admission
- low-power input intent declarations

Packages consume actions and normalized input values only.

---

## Core Principles

- Platform does not assign universal accept/back/action meanings.
- Package actions are symbolic and context-specific.
- Hardware inputs are mapped to package actions through Engine focus scopes.
- Platform/system policy may reserve, override, or suppress inputs.
- `BTN_BOOT` is never normal package input.
- Start shipping intent is power/save policy, not package input.
- Missing optional input capability must follow declared fallback behavior.
- Input focus must be explicit during scene transitions, suspend, resume, system interaction-state changes, and modal overlays.

---

## Logical Input Sources

Package-facing logical sources:

| Source | Meaning |
|---|---|
| `button` | logical Start/A/B/L/R button event where allowed |
| `button_chord` | Platform-classified button mask inside chord window |
| `button_hold` | logical held action derived from Platform button duration |
| `button_repeat` | logical repeat event where focus accepts repeats |
| `encoder_delta` | signed rotary movement with optional acceleration class |
| `joystick_direction` | cardinal/diagonal direction mask |
| `joystick_vector` | normalized deadzone-applied vector and magnitude |
| `wake_input` | input delivered through normal wake/resume lifecycle path |

Forbidden package-facing sources:

- GPIO level
- EXTI source
- timer counter
- encoder hardware count
- raw TMAG register value
- joystick calibration record
- button debounce state
- Start shipping threshold
- BOOT0/ROM boot behavior

---

## Action Model

Packages define symbolic actions.

Examples:

- `confirm`
- `cancel`
- `menu`
- `move`
- `look`
- `scroll`
- `adjust_value`
- `interact`
- `pause`
- `custom_action_1`

Action names are package-local. PeepOS may provide authoring templates, but templates compile to normal package actions.

Runtime consumes:

- action pressed
- action released
- action repeated
- action held
- action value changed
- action vector changed
- focus gained
- focus lost
- input capability unavailable

---

## Focus Scopes

Focus scopes define which package surface currently receives input.

Conceptual schema:

```text
focus_scope:
  scope_id
  priority
  modal_policy
  allowed_actions[]
  input_sources[]
  binding_table_ref
  repeat_policy
  chord_policy
  joystick_policy
  encoder_policy
  fallback_scope
```

Rules:

- one primary package focus scope is active per scene unless a bounded modal stack is declared.
- modal focus stacks must be bounded.
- focus changes must publish focus gained/lost events.
- focus loss must release or cancel active actions according to policy.
- shell/system overlays may preempt package focus.
- installer, maintenance, safe-mode, and Start shipping overlays may suppress package focus.

---

## System Interaction State Overlay

PeepOS owns a system interaction-state overlay above package focus. It is orthogonal to CPU awake/sleep state: an `ACTIVE` device may enter STOP between events, and an `INACTIVE` device may briefly wake to service system work without restoring package focus. Packages cannot disable system inactivity handling or author its timeout.

While `INACTIVE`:

- package focus is suspended
- only the target-owned system activation gesture is admitted for normal interaction
- HW6 initially uses Start; target policy may later admit another button or a classified chord such as `L+R`
- the physical activation gesture is consumed by PeepOS
- no package action is emitted for that same gesture
- on activation, PeepOS restores focus to the scene already established by the declared inactive route; `exit_to_shell` remains in shell
- Engine emits `DEVICE_INACTIVE` and `DEVICE_ACTIVE` through [[Runtime_Host_Contract]] after scene/focus ordering is valid; they are not input events

Allowed package inactive routes are:

- preserve the current scene
- transition to a declared scene
- exit to the PeepOS shell

A package may declare meaningful-activity sources and statically bounded inactivity deferral. Cosmetic animation and arbitrary activity hints cannot refresh the inactivity timer. Unbounded deferral is invalid. Packages may not define the physical system activation gesture; that remains target/system policy so wake-capable buttons and chords can evolve without changing package semantics.

---

## Binding Model

Bindings connect logical input sources to package actions.

Conceptual schema:

```text
input_binding:
  binding_id
  focus_scope
  source
  event_kind
  action
  modifiers
  repeat
  hold_threshold_ms
  deadzone_policy
  scale_policy
  fallback_action
```

Binding examples:

```text
A press -> confirm
B press -> cancel
START press -> menu
L+R chord -> custom_action_1
encoder delta -> scroll
joystick direction -> move
joystick vector -> aim
```

Rules:

- bindings use logical PeepOS input names only.
- bindings must be declared before package compilation/export.
- bindings must fit the selected target profile.
- Platform-reserved inputs may be rejected or overridden.
- ambiguous bindings must be resolved by focus priority and validation rules.
- package code must handle input capability rejection or fallback.

---

## Button Rules

Normal package-visible buttons:

- `START`
- `A`
- `B`
- `L`
- `R`

Reserved:

- `BOOT`
- Start shipping/power-intent behavior

Rules:

- `START` may be package input only while not consumed by system/power policy.
- long Start hold near shipping threshold is no longer normal package input.
- `BOOT` may never be bound by normal packages.
- button chords are logical button masks, not electrical timing controls.
- repeat requests are focus policy; Platform may clamp repeat cadence.

---

## Encoder Rules

Package-facing encoder input is logical delta.

Allowed data:

- signed delta
- acceleration class
- activity/idle event
- source mode such as normal or wake-delivered where visible

Rules:

- no package may read timer counters.
- no package may enable `ENC_EN`.
- encoder wake-armed behavior is a Platform policy decision.
- package focus may request encoder input capability or wake intent; Platform may grant, clamp, or reject.
- packages must provide fallback if encoder is optional.

---

## Joystick Rules

Package-facing joystick input is normalized only.

Allowed data:

- normalized X/Y vector
- deadzone-applied X/Y vector
- magnitude
- cardinal/diagonal direction mask
- active/inactive flag
- sample age
- calibration-valid flag

Rules:

- raw magnetic readings are diagnostics/calibration only.
- no package may read TMAG registers.
- package focus may request vector, direction, slow-poll, fast-poll, or low-power threshold intent.
- Platform may grant, clamp, or reject sample cadence and wake behavior.
- missing/invalid joystick calibration routes to Platform safe-mode behavior before normal package input.

---

## Low-Power And Wake Input

Packages express wake/input intent only.

Examples:

```text
wake_intent:
  button: START
  button: A
  joystick_direction
  encoder_activity
```

Rules:

- Platform decides which wake sources are armed.
- Platform decides sleep class compatibility.
- unsupported wake intent is rejected or downgraded by target profile.
- wake input is delivered through normal resume/lifecycle path.
- package code must tolerate delayed input after resume.
- low-power input intent must not bypass the PeepOS interaction-state overlay or Platform power policy.

---

## Scene Type Rules

| Scene Type Or Host | Input Rule |
|---|---|
| `STATE_SCENE` | event/wake-driven input handled by bounded reactive transactions; optional target-granted active-window cardinal joystick sampling |
| `SEQUENCE_SCENE` | active focus only for declared timeline controls; handlers must remain within the realtime frame budget |
| `PROGRAM_SCENE` | active focus for sandboxed program logic within declared frame and instruction budgets |
| `SHELL` | Platform-owned shell focus; package focus inactive unless launched |
| `INSTALLER` | installer/system focus; package input suppressed |

Focus and input requests must match the active scene. Scene transitions must release or transfer focus explicitly.

---

## Suggested API Shape

This is a conceptual C-level shape. Final signatures may differ, but must preserve the focus, action, and boundedness model.

```c
typedef enum {
    INPUT_OK = 0,
    INPUT_ERR_NOT_FOCUSED,
    INPUT_ERR_UNSUPPORTED,
    INPUT_ERR_RESERVED,
    INPUT_ERR_BOUNDS,
    INPUT_ERR_REVOKED,
    INPUT_ERR_INTERNAL
} input_result_t;

typedef enum {
    INPUT_EVENT_ACTION_DOWN,
    INPUT_EVENT_ACTION_UP,
    INPUT_EVENT_ACTION_REPEAT,
    INPUT_EVENT_ACTION_HOLD,
    INPUT_EVENT_ACTION_VALUE,
    INPUT_EVENT_ACTION_VECTOR,
    INPUT_EVENT_FOCUS_GAINED,
    INPUT_EVENT_FOCUS_LOST,
    INPUT_EVENT_CAPABILITY_UNAVAILABLE
} input_event_type_t;

typedef struct {
    uint32_t package_id;
    uint32_t scene_id;
    uint32_t focus_scope_id;
    uint32_t action_id;
    input_event_type_t event_type;
    int32_t value_x;
    int32_t value_y;
    uint32_t flags;
    uint32_t timestamp_ms;
} input_action_event_t;
```

Required API families:

- register scene input map
- activate focus scope
- push bounded modal focus scope
- pop focus scope
- query action state
- poll or receive input action events
- activate declared logical input context
- request low-power wake intent
- receive focus preemption notification
- receive input capability unavailable notification
- observe focus loss/restoration around the system interaction-state overlay; symbolic `DEVICE_INACTIVE`/`DEVICE_ACTIVE` delivery belongs to [[Runtime_Host_Contract]], not `input_event_type_t`

---

## Validation Requirements

Tooling and installer validation must reject:

- binding to `BTN_BOOT`
- binding to Start shipping/power-intent events
- hardware pin, GPIO, EXTI, timer, I2C, or register references
- focus stack depth exceeding target profile
- duplicate ambiguous bindings without resolution policy
- required input capability unavailable in target profile
- optional input capability without fallback
- sequence or program scene input path that blocks its realtime frame budget
- low-power wake intent unsupported by target profile
- package input map that cannot release focus on suspend/exit
- package interaction policy with an undeclared inactive route
- package-authored system activation gesture
- unbounded inactivity deferral or cosmetic animation declared as meaningful activity

---

## Digital Twin Requirements

The digital twin must simulate logical input through this contract.

Required behavior:

- replay button, encoder, joystick direction, and joystick vector traces.
- apply the same focus scopes and binding rules.
- inject capability unavailable cases.
- simulate wake-delivered input through the same lifecycle path.
- never expose host keyboard/controller scan codes as package runtime APIs.
- report deterministic action events in replay logs.

Host keyboard/gamepad bindings are twin/editor adapters only. They are not package input sources.

---

## Validation Cases

1. button binding maps to package action only while focus is active.
2. `BTN_BOOT` binding fails validation.
3. Start shipping intent is not delivered as normal package action.
4. the target-owned activation gesture is consumed and produces no package action.
5. the inactive overlay suppresses package focus until activation routing completes.
6. chord binding resolves through declared focus policy.
7. repeat request is clamped or ignored where target profile disallows it.
8. encoder delta maps to package action without exposing timer counters.
9. joystick vector/direction maps to package action without exposing raw registers.
10. scene transition releases or transfers focus explicitly.
11. shell/system overlay preempts package focus and package receives focus lost.
12. wake input resumes package through normal lifecycle before action delivery.
13. optional joystick unavailable path activates declared fallback.
14. digital twin replay produces deterministic action, inactive, active, and consumed-input event ordering.
15. package-authored system inactivity timeout or activation gesture fails validation.

---

Related:

- [[Button_Input_Contract]]
- [[Rotary_Encoder_Input_Contract]]
- [[Joystick_Hall_Input_Contract]]
- [[Package_Contract]]
- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Scene_Runtime_and_Interaction_Model]]
