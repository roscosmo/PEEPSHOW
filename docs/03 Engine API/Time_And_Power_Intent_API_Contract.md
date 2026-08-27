# Time And Power Intent API Contract

This document defines the Engine-facing time, cadence, lifecycle, wake, and power-intent API for PeepOS packages and game-development tools.

The API exposes logical time and intent. It does not expose RTC registers, SysTick, hardware timers, clock trees, STOP modes, wake-pin configuration, PMIC policy, LPBAM setup, DMA behavior, or RTOS scheduler internals.

Related:

- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Package_Contract]]
- [[Runtime_Host_Contract]]
- [[Scene_Runtime_and_Interaction_Model]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Power_and_Sleep_Policy]]
- [[PMIC_and_Power_Contract]]
- [[HW6_Brought_Up_Tracker]]

---

## Purpose

Games own logical time and state progression.

PeepOS owns physical timebases, RTC setup, wake scheduling, sleep entry, clock policy, inactivity timing, and the system `ACTIVE`/`INACTIVE` interaction state.

Package-facing time exists so packages can implement:

- virtual pet cycles
- day/night behavior
- real-world daily schedules
- cooldowns
- long-running scheduled state progression
- reactive state machines
- realtime scenes
- deterministic digital twin replay

Packages may read PeepOS calendar time and schedule against it. Packages may not set, correct, resync, or directly access RTC hardware.

---

## Ownership Boundary

The Platform owns:

- RTC hardware
- date/time setup UI
- time validity
- time correction and invalid-time recovery
- drift/resync policy
- local-time policy
- sleep class and clock transitions
- wake-source arming
- system inactivity timer and `ACTIVE`/`INACTIVE` enforcement
- quiesce/resume sequencing
- PMIC and shipping-mode policy

The Engine owns:

- package-facing logical time APIs
- runtime lifecycle event delivery
- schedule and delayed-event admission
- scene cadence validation
- power-intent validation
- digital twin time model contract
- package-visible wake reason normalization

Packages own:

- gameplay state progression
- reactions to local calendar time
- elapsed-time reconciliation after suspend/resume
- scheduled package events
- reactive wait contracts and meaningful-activity declarations
- declared scene-end, inactive, and failure routing to a `STATE_SCENE` or shell
- waiting-visual intent for settled reactive states
- declared inactive route, meaningful-activity sources, and bounded inactivity deferrals

Packages do not own physical time or power policy.

---

## Core Rules

- Calendar time is a normal package-facing primitive on target profiles that grant `time.calendar`.
- Packages may read local PeepOS date/time and schedule package events against it.
- Packages may not set or correct system time.
- Packages may not program RTC, SysTick, hardware timers, clocks, STOP mode, LPBAM, DMA, or wake pins.
- Packages may request logical schedules, realtime cadence, reactive waiting visuals, and symbolic wake intent only.
- `REACTIVE` logic runs in bounded event transactions and yields as soon as the transaction settles.
- Waiting for input or another event never requires an awake package loop.
- Waiting-display motion does not run package logic or advance committed game state.
- A package selects `CONTINUOUS` or `TIMEOUT`; PeepOS owns inactivity detection and the numeric timeout for `TIMEOUT`.
- A `TIMEOUT` package declares an inactive route and may defer inactivity only for statically bounded work.
- `SEQUENCE_SCENE` and `PROGRAM_SCENE` use `REALTIME`; each must declare suspend/resume behavior, and each in a `TIMEOUT` package must also declare meaningful activity and a route to a settled `STATE_SCENE` or shell.
- Runtime lifecycle events are the normal path for sleep/resume and active/inactive reconciliation.

---

## Calendar Time

PeepOS local calendar time is system-owned and package-readable.

Package-facing values:

```text
time.now_local()
time.date_local()
time.time_of_day_local()
time.weekday_local()
time.day_phase()
time.calendar_valid()
```

Examples of game use:

- morning/day/night behavior
- daily care routines
- birthday or anniversary events
- "come back tomorrow" mechanics
- shop/item rotations
- real-world cooldowns

Rules:

- normal package runtime may assume calendar time is valid on profiles that grant `time.calendar`.
- first setup should establish date/time before normal package runtime where `time.calendar` is part of the baseline profile.
- if calendar time becomes invalid later, PeepOS handles recovery through system lifecycle/setup/diagnostic flow.
- package code must not present its own RTC-setting path except through approved system UI requests.
- package save data that stores timestamps must treat them as PeepOS local calendar values with explicit schema versioning.

`day_phase` is a PeepOS helper derived from local time and system policy. Exact bands should be profile/system configurable, not hardcoded to a specific game.

---

## Elapsed Time

Packages also receive logical elapsed time.

Package-facing values:

```text
time.runtime_elapsed_ms()
time.frame_dt_ms()
time.elapsed_since(timestamp_or_saved_time)
resume_event.elapsed_suspended_ms
resume_event.elapsed_calendar_ms
```

Rules:

- `runtime_elapsed_ms` advances while a package scene is active.
- `frame_dt_ms` is supplied by the realtime host, not hardware timers.
- elapsed suspended time is delivered through lifecycle/resume context.
- reactive packages reconcile state from elapsed time instead of running continuously while suspended.
- elapsed time used for deterministic tests must be controllable by the digital twin.

---

## Scheduling

Packages may schedule logical events.

Package-facing operations:

```text
time.schedule_after(duration_ms, event_id)
time.schedule_at_local(local_datetime_or_rule, event_id)
time.cancel_schedule(event_id)
time.next_scheduled_event()
```

Schedule rule examples:

```text
after 10 seconds
after 30 minutes
at 07:00 local time
daily at 18:00 local time
next local date boundary
```

Rules:

- scheduled events are package events, not direct RTC alarms.
- Platform/Engine maps schedules onto safe wake/cadence behavior.
- reactive schedules may be coalesced or delayed according to target profile.
- tools must reject unbounded schedule tables.
- high-frequency schedules must be valid for the scene type and target profile.
- package schedules must survive suspend/resume through explicit package state or Engine schedule state.
- missed events after long sleep are delivered according to declared catch-up policy.

Catch-up policy examples:

| Policy | Meaning |
|---|---|
| `latest_only` | deliver only the latest due event |
| `count_elapsed` | deliver one event with elapsed occurrence count |
| `bounded_replay` | replay up to a declared maximum |
| `drop_if_stale` | discard stale event and continue |

Unbounded catch-up is invalid.

---

## Execution And Cadence Intent

Packages declare execution and cadence intent; Platform chooses physical timing and sleep behavior.

| Intent | Meaning |
|---|---|
| `REACTIVE` | bounded work occurs only in response to admitted events; runtime yields after settling |
| `REALTIME` | frame-paced execution remains active while the sequence or program scene is admitted |
| `reactive_scheduled_event_hz` | maximum requested cadence for logical scheduled state updates |
| `realtime_target_fps` | desired frame cadence for a sequence or program scene |
| `latency_tolerance` | acceptable response delay for package-visible work |

Rules:

- execution intent is derived from the active scene type and authored block
- execution intent and cadence are not requests for literal CPU frequency, voltage scale, PLL, or Platform operating point
- Platform may change its measured internal operating point without changing the package-visible meaning of `REACTIVE` or `REALTIME`
- `STATE_SCENE` always uses `REACTIVE`
- `SEQUENCE_SCENE` always uses `REALTIME` and executes a bounded, data-driven presentation timeline
- `PROGRAM_SCENE` always uses `REALTIME` and executes sandboxed package instructions
- reactive input work may be serviced promptly, then yields again
- reactive logic must not poll or request periodic ticks merely to animate the display
- a logical timer is valid only when game state or Engine behavior must advance
- waiting visuals are presentation intent and may be implemented autonomously without a package tick
- realtime frame pacing is valid only while a sequence or program scene remains admitted
- target profiles distinguish reactive event latency, scheduled logical cadence, and autonomous waiting-visual cadence

Target profile cadence limits include:

```text
cadence:
  reactive_scheduled_event_hz_max
  reactive_input_response_latency_ms_max
  realtime_target_fps
  realtime_frame_budget_ms

display.waiting_visual_animation:
  grant_status
  authored_frame_count_max
  cadence_hz_max
  cycle_duration_ms_max
  compiler_profile_id
  compiler_admission_required
```

These are validation and Platform policy limits. They are not hardware timer controls. Normal tools receive abstract compiler admission/utilization and fallback results, not panel-row, transfer-chunk, descriptor, SRAM4, or LPBAM limits.

---

## Lifecycle Events

Packages receive lifecycle events through the runtime host.

Package-facing lifecycle events:

```text
on_mount(context)
on_start(context)
on_suspend(reason)
on_resume(resume_context)
on_system_lifecycle(event)  # DEVICE_INACTIVE, DEVICE_ACTIVE
on_stop(reason)
on_unmount()
```

Resume context:

```text
resume_context:
  wake_reason
  elapsed_suspended_ms
  elapsed_calendar_ms
  calendar_valid
  power_context
  missed_schedule_summary
```

Rules:

- lifecycle delivery is ordered and bounded.
- `on_suspend` must not block indefinitely.
- `on_resume` is where reactive packages reconcile elapsed time and wake reason.
- package code must not assume it ran while suspended.
- `DEVICE_INACTIVE` and `DEVICE_ACTIVE` follow [[Runtime_Host_Contract]] ordering and are not input actions.
- resume failure routes through Engine lifecycle policy, not partial runtime state.

---

## Wake Reason

Packages receive normalized wake reasons where relevant.

Package-facing wake reasons:

| Wake Reason | Meaning |
|---|---|
| `input` | button, encoder, joystick, or focus-delivered input wake |
| `schedule` | package/system schedule or RTC cadence wake |
| `sensor` | approved sensor event wake |
| `power` | PMIC, charger, battery, or shipping/power event |
| `usb` | USB attach/detach or installer/system event |
| `fault_recovery` | watchdog or recovery path |
| `system` | shell/system transition |
| `unknown` | Platform defect path; not normal gameplay |

Rules:

- raw EXTI, RTC alarm IDs, PMIC registers, and wake pins are not package-visible.
- unknown wake reasons are Platform defects until explained.
- HW6 communication cannot be a wake reason for packages until a measured HW6 profile grants it.
- wake input is delivered through lifecycle/resume before normal package action delivery where required by input policy.

---

## Reactive Wait And Meaningful Activity

Reactive blocks publish the next wait contract rather than requesting that hardware stay awake.

Conceptual operations:

```text
power.publish_reactive_wait(wait_contract_id)
power.mark_meaningful_activity(activity_source)
power.realtime_work_pending(reason)
power.defer_inactivity_until(bounded_completion_id)
```

A reactive wait contract resolves:

- waiting-visual intent and fallback
- admitted event interests
- logical schedules/deadlines
- symbolic wake intents
- package gameplay-timeout transitions
- interaction context reference for predeclared meaningful activity or bounded inactivity deferral

Rules:

- after a reactive transaction settles, the host yields immediately
- activity declarations do not directly keep hardware awake
- only declared admitted sources may refresh the system inactivity timer
- passive animation, autonomous playback, keepalives, and arbitrary activity hints are not meaningful user activity
- gyro or another non-button source may be meaningful activity when declared by the active block and granted by the target profile
- bounded non-interruptible work may defer inactivity until completion or a validated timeout
- unbounded inactivity deferral is invalid
- `SEQUENCE_SCENE` and `PROGRAM_SCENE` must declare suspend/resume behavior; in a `TIMEOUT` package they also declare meaningful activity and a route to a settled `STATE_SCENE` or shell

### System Interaction State

PeepOS provides a system interaction policy independent of whether the CPU is awake or in STOP. A package selects `CONTINUOUS` or `TIMEOUT`; `CONTINUOUS` remains `ACTIVE`, while `TIMEOUT` uses:

- `ACTIVE`: package focus and admitted package input are available.
- `INACTIVE`: package focus is suspended and only system-admitted activation gestures are accepted.

The package declares:

```text
interaction_policy:
  mode                       # continuous, timeout
  meaningful_activity_sources[]
  inactive_route             # preserve_scene, transition_to_scene, exit_to_shell
  inactive_target_scene      # required for transition_to_scene
  inactive_waiting_visual_ref
  bounded_deferrals[]
```

PeepOS owns the `TIMEOUT` interval, timer enforcement, input suppression, activation gesture, wake-source arming, system cue, activation animation, and lifecycle ordering. The HW6 baseline activation gesture is `START`. Target policy may later admit another button or a classified chord such as `L+R`; packages do not choose raw buttons or chords for this system action. The gesture that activates the device is consumed by PeepOS and is not delivered as a package action. While inactive, HW6 may consume A/B/L/R to show a bounded `PRESS START` cue without activating the package; cue expiry restores the inactive waiting presentation and STOP eligibility, and joystick movement wake remains disarmed. `START` activates directly whether or not that cue is visible; HW6 then shows one bounded PeepOS-owned eye-opening overlay before revealing the restored active presentation. Engine emits `DEVICE_INACTIVE` after the declared inactive route settles and emits `DEVICE_ACTIVE` after runtime state and focus are ready, following [[Runtime_Host_Contract]].

Package-authored inactivity, such as leaving explore mode after 30 seconds, is represented by `schedule_after` plus a normal scene transition. It is independent of the system interaction-state timer.

---

## Interactive Session Wait

Some communication experiences need the local device to remain responsive while a remote peer is taking a turn or supplying the next meaningful session action.

The communication contract may declare an interactive wait policy for that context. Power policy may then grant a bounded peer-wait grace from the target profile.

Rules:

- peer-wait treatment cannot bypass system inactivity policy, create unbounded inactivity deferral, or prevent a reactive host from yielding.
- packages do not author the grace duration through the power API.
- meaningful remote activity may refresh peer-wait grace only when the communication context and target profile allow it.
- keepalives, presence chatter, and arbitrary activity hints are not a general stay-awake path.
- when the admitted wait expires, the runtime receives the declared session event and follows its reactive transition or fallback.
- HW6 communication still cannot wake a package after low-power entry unless a future measured HW6 profile grants it.

---

## Scene Type Rules

| Scene Type | Execution | Time And Power Behavior |
|---|---|---|
| `STATE_SCENE` | `REACTIVE` | bounded event/schedule/input transactions; no polling or awake wait loop; settled presentation may continue through an admitted autonomous backend |
| `SEQUENCE_SCENE` | `REALTIME` | frame-paced, data-driven timeline; must declare suspend behavior and a route to a settled state or shell |
| `PROGRAM_SCENE` | `REALTIME` | frame-paced sandboxed instructions; must declare budgets, suspend behavior, and a route to a settled state or shell |

Settled reactive presentation:

- a state may hold its committed frame or describe bounded waiting motion
- package logic does not execute while waiting
- Platform derives static hold, autonomous playback, or another admitted backend
- failed autonomous admission follows the declared reduced-visual or hold fallback

---

## Tool-Time Validation

Tooling must validate time and power intent before package compilation/export.

Reject:

- direct RTC, SysTick, hardware timer, STOP, clock, PMIC, or wake-pin references.
- unbounded schedule tables.
- unbounded catch-up.
- polling loops used to approximate reactive schedules or waiting-visual cadence.
- high-frequency schedules or awake input-wait loops in state scenes.
- reactive state without a waiting visual/event contract.
- interaction policy with an undeclared inactive route.
- package-authored system inactivity timeout or activation gesture.
- unbounded inactivity deferral or cosmetic animation declared as meaningful activity.
- sequence or program scene in a `TIMEOUT` package without meaningful-activity rules and a route to a settled state or shell.
- interactive session wait without target-profile support or a declared expiry route.
- scene that requests cadence above target profile caps.
- package wake intent unsupported by selected target profile.
- communication wake intent on HW6 profiles while the capability is blocked.
- package requiring `time.calendar` on a target profile that does not grant it.
- package timestamp fields without schema type/version where persisted.

Authoring tools should explain failures in PeepOS terms, such as:

```text
This state scene requests a 20 Hz logical schedule. Use a sequence or program scene, or lower the cadence.
```

They should not expose RTC, STOP, clock, timer, or wake-pin implementation details to normal game authors.

---

## Digital Twin Requirements

The digital twin must use the same time and power-intent contract as the hardware runtime.

Required twin time models:

- deterministic fixed-step
- interactive wall-clock

Optional twin time models:

- accelerated reactive-wait simulation
- single-step event evaluation
- recorded timeline replay

Rules:

- calendar time must be controllable in deterministic tests.
- scheduled events, lifecycle events, wake reasons, cadence clamps, reactive yields, interaction-state timing, consumed activation gestures, and admitted interactive peer-wait behavior must be replayable.
- twin profiles must derive cadence caps and interaction-policy bounds from measured/frozen target profiles.
- twin evidence does not prove RTC hardware, wake latency, current draw, or physical sleep behavior.

---

## Validation Cases

1. package can read valid PeepOS local calendar time without RTC access.
2. package cannot set, correct, resync, or directly program RTC.
3. first-setup or recovery flow establishes valid system time before launching calendar-dependent packages.
4. `schedule_after` and `schedule_at_local` produce bounded package events.
5. long sleep resumes package with elapsed suspended/calendar time and bounded missed-event summary.
6. unbounded catch-up policy fails validation.
7. state scene with polling or an awake input-wait loop fails validation.
8. sequence or program scene in a `TIMEOUT` package without meaningful-activity rules and a route to a settled state or shell fails validation.
9. reactive transaction yields after its bounded state/action/render work settles.
10. package-authored gameplay inactivity produces a normal scheduled transition and does not mutate the PeepOS interaction-state timer.
11. `CONTINUOUS` remains active; `TIMEOUT` enters the system inactive lifecycle; neither mode authors the numeric timeout.
12. `INACTIVE` suppresses normal package controls except the target-owned activation gesture.
13. a completed inactive route emits `DEVICE_INACTIVE`; the activation gesture is consumed and `DEVICE_ACTIVE` is delivered only after state/focus restoration.
14. preserve-scene, declared-scene-transition, and shell-exit inactive routes validate.
15. bounded inactivity deferral validates; unbounded deferral fails.
16. admitted gyro activity may refresh the inactivity timer only when declared and granted.
17. autonomous waiting visuals do not advance committed package state.
18. HW6 communication wake intent fails validation until a measured HW6 profile grants it.
19. digital twin deterministic replay produces the same schedule, reactive-yield, inactive/active, wake, lifecycle, and event sequence for a fixed trace.
20. digital twin accelerated sleep simulation is not used as physical-target current, wake-latency, or RTC hardware evidence.
21. package-authored system inactivity timeout or activation gesture fails validation.
