# PeepOS Capability Registry

This document defines Engine-visible PeepOS capability names for game-authoring tools, packages, compatibility checks, and the digital twin.

Capabilities are abstract. They do not name pins, buses, peripherals, DMA channels, STM32 HAL handles, RTOS objects, or hardware registers.

Related:

- [[Game_Authoring_API_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Runtime_Host_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Package_Contract]]
- [[Power_and_Sleep_Policy]]
- [[Display_and_Rendering_Contract]]
- [[Audio_Contract]]
- [[Audio_API_Contract]]
- [[Input_Index]]
- [[Sensors_Index]]
- [[Sensor_API_Contract]]
- [[Communication_Index]]
- [[Communication_API_Contract]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Diagnostics_API_Contract]]
- [[HW6_Brought_Up_Tracker]]

---

## Capability Status

Every capability has a status.

| Status | Meaning |
|---|---|
| `CONTRACTED` | Engine-facing shape is documented, but hardware behavior may still require validation before shipping use. |
| `HW_VALIDATED` | corresponding behavior has target-qualified measured evidence; the selected target profile still determines whether it is granted. |
| `PROFILE_OPTIONAL` | package may target it only when the selected target profile grants it or a fallback is declared. |
| `EXPERIMENTAL` | may be used in bring-up, dev, or preview profiles only; shipping packages must not require it. |
| `BLOCKED` | not available to packages until the contract or hardware evidence changes. |

Pre-validation documents may define `CONTRACTED` capabilities so tools can be designed, but physical support is not known-good for a target until target-qualified evidence exists and its profile grants the capability.

---

## Fallback Rules

Required capabilities must be available in the selected target profile.

Optional capabilities require fallback behavior.

Fallback behavior may include:

- disable the feature
- substitute static content
- use a lower cadence
- use a simpler input path
- use silent audio behavior
- run local-only instead of multiplayer
- use a default/resolved sensor value for optional content behavior

Fallbacks must be validated before package compilation/export.

---

## Runtime Capabilities

Runtime class is primarily declared in the manifest, but tools may still use these names in compatibility reports.

| Capability | Status | Meaning |
|---|---|---|
| `runtime.lp_graph` | `CONTRACTED` | bounded reactive event/state graph execution |
| `runtime.lp_module` | `CONTRACTED` | Engine-hosted bounded reactive module execution |
| `runtime.rt_scene` | `CONTRACTED` | frame-paced realtime scene execution |

`SHELL` and `INSTALLER` are Platform-owned runtime classes, not normal package target capabilities.

---

## Runtime Logic Capabilities

| Capability | Status | Meaning | Fallback Required If Optional |
|---|---|---|---|
| `logic.state_graph` | `CONTRACTED` | bounded state/substate graph execution | no |
| `logic.action_table` | `CONTRACTED` | bounded symbolic action tables | no |
| `logic.guards` | `CONTRACTED` | bounded guard/expression evaluation | no |
| `logic.lifecycle_events` | `CONTRACTED` | package-visible lifecycle event delivery | no |
| `logic.calendar_events` | `CONTRACTED` | local-calendar schedule events through time contract | yes if optional |
| `logic.realtime_frame_tick` | `CONTRACTED` | `RT_SCENE` frame-paced update event with declared budget | yes outside realtime units |
| `logic.deterministic_replay` | `CONTRACTED` | deterministic replay of runtime logic in host/digital twin profiles | yes |

Runtime logic capabilities are defined by [[Runtime_Logic_State_API_Contract]]. They do not imply threads, RTOS timers, hardware callbacks, dynamic code loading, or direct Platform access.

---

## Display Capabilities

| Capability | Status | Meaning | Fallback Required If Optional |
|---|---|---|---|
| `display.mono_canvas` | `CONTRACTED` | logical 1-bit drawing surface matching the active target profile | no for the PeepShow package baseline |
| `display.static_hold` | `CONTRACTED` | last frame remains visible while package is idle or suspended | no |
| `display.reactive_update` | `CONTRACTED` | bounded display updates produced by reactive event transactions | no |
| `display.realtime_frame` | `CONTRACTED` | frame-paced display requests for active realtime scenes | yes for non-realtime packages |
| `display.waiting_visual_animation` | `HW_VALIDATED` | bounded waiting visuals can continue while reactive package logic is yielded; HW5 backend evidence uses LPBAM/LPDMA | yes |

Display changed-region tracking, transfer selection, DMA, and LPBAM setup are Engine/Platform internals and are not package capabilities.

`display.waiting_visual_animation` has measured HW5 evidence. HW6 support remains pending revalidation. Shipping use still requires a frozen profile for the selected target that grants measured cadence, compiler-admission, wake, and recovery behavior without exposing Platform row/chunk mechanics to normal package tools.

---

## Rendering Capabilities

| Capability | Status | Meaning | Fallback Required If Optional |
|---|---|---|---|
| `render.layered_compositor` | `CONTRACTED` | bounded `UI -> GAME -> BG` layer compositing | no |
| `render.masked_1bpp` | `CONTRACTED` | black/white sprite or image assets with opacity mask | no |
| `render.tone5_coverage` | `CONTRACTED` | semantic tone5 assets resolved to 1-bit coverage patterns | yes if optional |
| `render.integer_scale` | `CONTRACTED` | integer-scaled sprite/tone rendering within target profile limits | yes if optional |
| `render.tilemap_viewport` | `CONTRACTED` | bounded tilemap region/viewport rendering | yes if optional |
| `render.waiting_visual_sequence` | `CONTRACTED` | package may contain bounded waiting-visual sequence assets | yes |

`render.waiting_visual_sequence` means the package can carry validated preferred/fallback waiting visuals. Continued display motion while reactive logic is yielded requires `display.waiting_visual_animation`; tools derive that requirement from the authored wait contract rather than exposing LPBAM.

`tone5` is a semantic coverage model. It is not native display color and must not be described as a color-depth format.

---

## Input Capabilities

| Capability | Status | Meaning | Fallback Required If Optional |
|---|---|---|---|
| `input.buttons` | `CONTRACTED` | logical button actions and chords | no for the PeepShow package baseline |
| `input.encoder` | `CONTRACTED` | logical encoder delta actions | yes if optional |
| `input.joystick_vector` | `CONTRACTED` | normalized joystick vector/action data | yes if optional |
| `input.joystick_direction` | `CONTRACTED` | normalized cardinal/diagonal direction data | yes if optional |
| `input.focus` | `CONTRACTED` | Engine focus scopes and action routing | no |
| `input.chords` | `CONTRACTED` | logical button chord bindings through focus policy | yes if optional |
| `input.hold_repeat` | `CONTRACTED` | logical hold and repeat action delivery where policy allows | yes if optional |
| `input.low_power_wake_intent` | `CONTRACTED` | package may declare logical input wake intent | yes if optional |
| `input.optional_auto_lock` | `CONTRACTED` | package may enable or disable PeepOS automatic input locking and choose an admitted lock route | no |
| `input.start_unlock_consumed` | `CONTRACTED` | Start used to unlock is consumed by PeepOS; Engine emits `DEVICE_UNLOCKED` instead of a package Start action | no |

`BTN_BOOT` is not a game capability.

Start shipping intent is not a game capability.

Input capabilities are logical. GPIO, EXTI, timer counters, I2C registers, raw joystick magnetic data, debounce state, and wake-pin configuration are not package capabilities.

HW6 has no rotary encoder. Every HW6 profile must set `input.encoder_supported = false` and block `input.encoder`; the registry entry remains for portable packages and other targets.

---

## Audio Capabilities

| Capability | Status | Meaning | Fallback Required If Optional |
|---|---|---|---|
| `audio.music` | `CONTRACTED` | symbolic music cue requests | yes if optional |
| `audio.sfx` | `CONTRACTED` | symbolic SFX cue requests | yes if optional |
| `audio.bbb` | `CONTRACTED` | bounded BBB tone, sweep, and pattern requests | yes if optional |
| `audio.volume_intent` | `CONTRACTED` | package may request volume/mute intent through Engine policy | no |
| `audio.timeline` | `CONTRACTED` | symbolic cue timeline events for diagnostics, replay, or package logic where supported | yes if optional |

Audio is a creative package primitive. PeepOS does not require packages to remain semantically complete when muted.

Physical output may be muted, suppressed, degraded, or quarantined by Platform policy. Packages consume symbolic audio APIs through [[Audio_API_Contract]] and must not control audio hardware directly.

HW6 has no PAM/piezo path. Every HW6 profile must set `audio.bbb_supported = false` and block `audio.bbb`; music and SFX remain independently profile-gated through the retained speaker path.

---

## Asset Capabilities

| Capability | Status | Meaning |
|---|---|---|
| `asset.sprites` | `CONTRACTED` | sprite/image assets addressed by ID |
| `asset.masked_1bpp_sprites` | `CONTRACTED` | black/white masked sprite assets addressed by ID |
| `asset.tone5_sprites` | `CONTRACTED` | tone5 masked sprite assets addressed by ID |
| `asset.tilemaps` | `CONTRACTED` | bounded tilemap/map assets addressed by ID |
| `asset.tilesets` | `CONTRACTED` | bounded tileset assets addressed by ID |
| `asset.animations` | `CONTRACTED` | bounded animation tables |
| `asset.fonts` | `CONTRACTED` | bounded font assets and text layout metadata |
| `asset.text` | `CONTRACTED` | text/localization tables |
| `asset.data_tables` | `CONTRACTED` | bounded package data tables |
| `asset.waiting_visual_sequences` | `CONTRACTED` | bounded portable waiting-visual sequence assets |

Asset capabilities are package-data capabilities. They do not imply filesystem access.

---

## Save Capabilities

| Capability | Status | Meaning |
|---|---|---|
| `save.records` | `CONTRACTED` | schema-versioned save record read/write |
| `save.migration` | `CONTRACTED` | approved save migration path |
| `save.reset` | `CONTRACTED` | explicit package-owned save reset flow |
| `save.package_settings` | `CONTRACTED` | schema-defined package-owned settings |
| `save.write_budget` | `CONTRACTED` | package declares bounded write policy and frequency assumptions |

Saves and package settings are not files. Packages access them only through [[Package_Save_Settings_API_Contract]].

Platform settings, calibration, BLE bonding, install metadata, and fault logs are not package save capabilities.

---

## Time And Power Intent Capabilities

| Capability | Status | Meaning |
|---|---|---|
| `time.delayed_event` | `CONTRACTED` | bounded delayed event requests |
| `time.calendar` | `CONTRACTED` | valid PeepOS local date/time read access for packages |
| `time.calendar_schedule` | `CONTRACTED` | bounded package schedules against local date/time rules |
| `time.frame_delta` | `CONTRACTED` | realtime host frame delta for active realtime units |
| `time.wake_reason` | `CONTRACTED` | normalized package-visible wake reason through lifecycle |
| `time.catch_up_policy` | `CONTRACTED` | bounded missed-event reconciliation policy |
| `time.rtc_wake_intent` | `CONTRACTED` | RTC-backed wake/cadence intent without RTC hardware control |
| `power.reactive_wait` | `CONTRACTED` | package can declare a settled reactive wait contract without selecting sleep hardware |
| `power.latency_hint` | `CONTRACTED` | package declares acceptable response latency |
| `power.cadence_request` | `CONTRACTED` | package can request bounded reactive schedule cadence or realtime frame cadence |
| `power.meaningful_activity` | `CONTRACTED` | package can report activity from an admitted declared source without directly controlling CPU residency |
| `power.reactive_fallback` | `CONTRACTED` | package can declare fallback routing from realtime work to a bounded reactive unit/state |

Packages may read PeepOS calendar time where granted, but may not set, correct, resync, or directly access RTC hardware.

These capabilities express intent only. Platform chooses RTC setup, sleep class, clocks, wake-source arming, and resume policy.

Target profiles expose package-facing wake behavior as wake intents and normalized lifecycle wake reasons. Hardware wake-source arming remains Platform policy.

---

## Sensor Capabilities

| Capability | Status | Meaning | Fallback Required If Optional |
|---|---|---|---|
| `sensor.light` | `CONTRACTED` | resolved ambient-light value and band | yes if optional |
| `sensor.light_stream` | `CONTRACTED` | bounded active light sampling context where supported | yes if optional |
| `sensor.imu_steps` | `CONTRACTED` | step session, step count snapshot, or delta | yes if optional |
| `sensor.imu_events` | `CONTRACTED` | motion, tap, shake, tilt, or orientation events where supported | yes if optional |
| `sensor.imu_motion_snapshot` | `CONTRACTED` | normalized low-rate motion/orientation snapshot | yes if optional |
| `sensor.imu_motion_stream` | `CONTRACTED` | bounded higher-rate motion context for realtime gameplay | yes if optional |

Sensor raw values are diagnostics/calibration only. Packages consume resolved PeepOS values, sessions, contexts, and events through [[Sensor_API_Contract]].

Each validated profile publishes only the sensors physically present on that target. HW6 must set `sensors.light_supported = false` and omit `sensor.light` and `sensor.light_stream`; its retained IMU capabilities remain pending HW6 validation. A fault in a granted sensor is handled through Platform/Engine fault lifecycle and diagnostics, not normal package gameplay logic.

---

## Communication Capabilities

| Capability | Status | Meaning | Fallback Required If Optional |
|---|---|---|---|
| `comm.multiplayer` | `CONTRACTED` | generic multiplayer session and bounded messages | yes if optional |
| `comm.companion` | `CONTRACTED` | generic companion-app session and bounded messages | yes if optional |
| `comm.local_loopback` | `PROFILE_OPTIONAL` | host/digital-twin or diagnostic loopback capability | yes |
| `comm.session_required` | `CONTRACTED` | runtime unit may require an active communication session for admission | no if declared as required |
| `comm.message_schema` | `CONTRACTED` | bounded versioned package communication message schemas | no |

Communication contexts are transport-agnostic. Packages consume abstract sessions, peers, and bounded messages through [[Communication_API_Contract]].

Each communication runtime unit must declare either fallback/route behavior or session-required admission behavior.

HW5 profiles did not grant communication wake. HW6 profiles must also leave it unavailable until target-qualified measurement explicitly grants it.

---

## Diagnostics Capabilities

| Capability | Status | Meaning |
|---|---|---|
| `diag.markers` | `CONTRACTED` | package may emit lightweight bounded markers |
| `diag.counters` | `CONTRACTED` | package may emit bounded counters |
| `diag.timing` | `CONTRACTED` | package may emit bounded timing scopes in approved profiles |
| `diag.trace_values` | `CONTRACTED` | package may emit bounded structured values in dev/twin profiles |
| `diag.package_fault` | `CONTRACTED` | package may emit package fault codes routed through Engine lifecycle |
| `diag.replay_markers` | `CONTRACTED` | deterministic replay markers for host/digital-twin tests |
| `diag.shipping_minimal` | `CONTRACTED` | shipping package may retain minimal bounded diagnostic evidence |

Diagnostics are rate-limited and do not own debug transports. Package diagnostics are defined by [[Diagnostics_API_Contract]].

---

## Target Profiles

Target profiles grant a concrete set of capabilities and limits.

The authoritative target profile schema is defined in [[Target_Profile_Schema_Contract]].

Required initial profiles:

| Profile | Purpose |
|---|---|
| `HW6_PENDING_VALIDATION` | active design-time profile before measured HW6 evidence; not shipping-authoritative |
| `HW6_VALIDATED_BASELINE` | future measured HW6 Platform behavior without granting autonomous waiting visuals |
| `HW6_VALIDATED_LPBAM` | future measured HW6 Platform behavior with the revalidated LPBAM waiting-visual backend |
| `HOST_AUTHORING_PREVIEW` | editor/simulator preview with mocks and placeholders |
| `HOST_DIGITAL_TWIN_HW6` | future host twin profile derived from measured HW6 behavior after validation |
| `HW5_PENDING_VALIDATION` | retired HW5 design profile; deprecated |
| `HW5_VALIDATED_BASELINE` | retired HW5 measured baseline profile; historical compatibility only |
| `HW5_VALIDATED_LPBAM` | retired HW5 measured LPBAM profile; historical compatibility only |
| `HOST_DIGITAL_TWIN_HW5` | retired HW5-derived host profile; historical compatibility only |

Profiles must record:

- capability grant list
- capability status list
- runtime classes
- runtime logic limits
- event queue and transition stack limits
- time/calendar profile
- display profile
- rendering profile
- cadence limits
- reactive-wait contract and immediate-yield requirement
- optional input-lock bounds and admitted lock routes
- reactive scheduled-event cadence cap
- reactive input-response latency cap
- baseline wake/update/yield policy
- realtime frame budget
- realtime target frame rate
- wake intents and lifecycle wake reasons
- waiting-visual animation availability
- waiting-visual compiled payload/chunk/cadence caps, if available
- input availability
- sensor primitives and context limits
- audio limits
- communication limits
- save/storage limits
- Platform contract revision
- evidence reference when hardware-derived

---

## Capability Change Control

Adding or changing a capability requires:

1. update this registry
2. update [[Game_Authoring_API_Contract]] if authoring behavior changes
3. update package schema or compatibility reports if serialized data changes
4. update digital twin profile rules if host behavior changes
5. update Platform contract or hardware validation docs if hardware behavior is affected

Reference Game needs may request new capabilities, but accepted capabilities must be reusable beyond the Reference Game.
