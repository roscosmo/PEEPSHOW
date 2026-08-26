# HW6 Revalidation Matrix

This matrix defines which HW5 results may guide HW6 testing and what must be
remeasured before the active HW6 target profile is published.

Status: `bringup_in_progress_reconciled_2026_08_27`

## Evidence Tiers

| Tier | Meaning | HW6 Rule |
|---|---|---|
| `A` | safety, power, recovery, clock, sleep, or target-profile critical | repeat completely on every initially characterized HW6 unit |
| `B` | retained subsystem with matching intended topology | repeat bounded functional and power regression on HW6 |
| `C` | host/tool/contract behavior independent of physical revision | reuse only after HW6 target limits and capability flags are substituted |
| `N/A` | physical capability removed from HW6 | mark unavailable; do not emulate as hardware proof |

## Matrix

| Area | Tier | HW5 Use | Required HW6 Evidence | Status |
|---|---|---|---|---|
| board identity and assembly intake | A | checklist structure only | photos, revision IDs, board ID, assembly lot, rework state | `partial_unit_001` |
| unpowered shorts and continuity | A | expected-net guide | measured resistance/continuity on received unit | `pending` |
| ADP5360 rails and faults | A | register sequence hypothesis | first-power current, 1.8/3.3 V rails, PGOOD/fault, safe configuration | `partial_unit_001`; full `0x00..0x36` map and guarded reversible profile transaction pass; exact candidate/restored readback, PGOOD `0x07`, fault `0x00`, and VBUS-absent state proven; persistent profile, JEITA hot/cold/cool/warm behavior, protection thresholds, VBUS current limit, and charge-current/termination evidence remain pending; FW0 validates fuel-gauge prepare/runtime threshold reads, runtime warning/critical/recovery, no-VBUS boot/restart blocking below restart-allow, VBUS-present boot charge recovery below restart-allow, UI recovery above restart-allow, initial real-cell charger/VBUS state with `0x0A=0x80`, THR OK, ADP5360/PA9 VBUS agreement, active fast-charge reporting, charger/VBUS-safe PMIC_INT handling with MCU `PB15` pull-up, guarded EXTI15 arming, `0x32=0x03`, write-one-clear flag handling, `thPower` snapshot consumption, and the guarded ADP5360 `0x36` software shipment primitive |
| reset, BOOT0, SWD recovery | A | recovery procedure | attach-under-reset, ID, halt/reset, safe flash recovery | `partial_unit_001`; SWD identity, safe flash, and attach-under-reset proven |
| diagnostic timing output / PPK2 logic | A | HW5 timing-marker method only | `PWR_DBG` default, physical route, logic reference, edge timing | `pass_unit_001`; idle-low baseline, physical route, prior 250 ms heartbeat, and bounded owner-workflow high/low marker proven |
| clock tree and operating points | A | candidate points only | generated register audit, clock outputs/derived timings, reactive/realtime sweeps | `pending` |
| hardwired display translator | A | HW5 `PD2 VLT_LCD` control behavior is not transferable | confirm translator is continuously enabled and EXTCOMIN reaches the powered panel in active and STOP contexts | `partial_unit_001`; active-mode diagnostic frame remained visible with RTC calibration output enabled through the display-owner lifecycle; STOP-context electrical confirmation remains pending |
| static display transfer | B | expected mapping and patterns | HW6 polarity, row order, byte order, full/partial transfer | `partial_unit_001`; full-frame orientation and package-backed masked 1bpp rendering are visually proven, while panel-native dirty-row updates drive awake GPDMA and LPBAM preparation; exhaustive polarity/row/byte regression and display fault injection remain pending |
| LPDMA and SRAM4 | A | sizing hypothesis | HW6 map report, reachability, alignment, maximum reliable transfer | `partial_unit_001`; SRAM4 autonomously serves a fixed `10512`-byte payload arena in `18` equal slots, with six 28-row transactions per panel state and three guaranteed full-panel states; coherent 126-node playback completed on target; release map/profile freeze and deliberate future optimization remain pending |
| LPBAM autonomous display | A | validated experiment design | STOP2 playback, cadence, coherent logical frames, wake/abort, seeded handoff, current | `partial_unit_001`; renderer-owned 250 ms waiting visuals, mixed 2/3-phase six-step playback, coherent multi-chunk frames, deterministic three-state fallback, phase-preserving entry/wake, repeated STOP2 cycles, and seamless handoff are target-proven; active-transaction wake capture, wider payload/current scaling, fault injection, and target-profile promotion remain pending |
| external flash | B | command baseline | JEDEC/status, scratch erase/program/readback, deep-power-down | `partial_unit_001`; JEDEC `1f 42 18`, polling and DMA scratch operations, raw block adapter, fixed region layout, FileX/LevelX staging, deep-power-down/recovery, USB MSC, persistent calibration records, and persistent A/B package/index operations are target-proven; timing/current limits, reset-stage install injection, wear behavior, and general fault recovery remain pending |
| speaker audio | B | sample/DMA baseline | 16 kHz clock, SAI DMA, audible output, current, clean shutdown | `partial_unit_001`; driver-backed 4.096 MHz kernel, 16 kHz SAI DMA, audible three-run 1 kHz tone, zero DMA/SAI error, and `SD_MODE` return-low pass; current, refill/underrun, mixer, and fault behavior remain pending |
| buttons and BOOT0 | A | logical mapping baseline | levels, EXTI, debounce, wake, activation-gesture consumption, boot boundary | `partial_unit_001`; START and A/B/L/R STOP2 wake, ordered edge delivery, zero-drop stress, and one logical action per accepted press are target-proven; BOOT-button application behavior, system activation consumption, and non-START hold/repeat/chord/stuck policy remain pending |
| joystick / TMAG3001 | B | driver and threshold baseline | identity, calibration, axis mapping, wake interrupt, sleep current | `partial_unit_001`; persistent guided calibration, normalized vector, canonical diagonal detection, deterministic four-way shell resolution, four logical cardinal events, X/Y omnipolar STOP2 wake for every cardinal, bounded post-wake direction confirmation, and matched five-minute `55 uA` disabled / `65 uA` enabled measurements are target-proven; neutral false-wake soak, per-scene four/eight-way policy, hold/repeat policy, and PROGRAM vector polling remain pending |
| IMU / LIS2DUX12 | B | driver and sleep baseline | identity, events, lowest-power mode, interrupt, current | `partial_unit_001`; identity at `0x18`, terminal deep-power-down entry, expected I2C wake NACK acceptance, `WHO_AM_I=0x47`, low-rate `CTRL5=0x10`, and two owner-routed wake/suspend cycles pass; active sensing, embedded functions, interrupt/event wake, step-counter retention, and current remain pending |
| NINA UART and flow control | B | command sequence baseline | boot, identity, RTS/CTS, command/data mode, sleep/wake | `pending` |
| BLE and NFC | B | HW5 experiment procedure | advertising, pairing/session, SPS TX/RX, NFC read, current | `pending` |
| USB MSC / storage ownership | B | recovery procedure | attach/enumeration, mount/reclaim, package install/launch, disconnect recovery, power | `partial_unit_001`; host mount/read/write/delete, repeated export/reclaim, staging scan, persistent A/B package programming, commit-last index selection, installed-RAM publication, immediate launch, and selected-generation reload are target-proven; pre-commit full semantic validation, reset-stage injection, boot activation, uninstall/fallback, and final power/fault evidence remain pending |
| RTOS owner topology | B | code/queue design | owner creation, queue routing, quiesce acknowledgements, fault paths | `partial_unit_001`; all objects/startup envelopes plus real power-to-display/audio routing, bounded acknowledgements, serialized PMIC lease, sole-owner physical actions, and two lifecycle-v10 driver-backed wake/revalidate/quiesce cycles pass for LIS2DUX12, TMAG3001, display, audio, and polling scratch flash; saturation, fault injection, cancellation, STOP2 handoff, and production message contracts remain pending |
| STOP2 and wake matrix | A | expected sequence | every HW6 wake source, unknown wake handling, resume, repeated cycles | `partial_unit_001`; automatic STOP2, START and A/B/L/R wake, joystick movement wake on all four cardinals, owner quiesce/resume, display handoff, and repeated cycles are target-proven; RTC, BOOT, IMU, PMIC STOP2 classification, USB wake, unknown-source fault injection, and long soak remain pending |
| waiting current | A | approximate goal only | instrumentation-minimized HW6 current by backend/context | `measured_unit_001_partial_matrix`; five-minute means include `51.39 uA` held frame, approximately `56 uA` sparse 250 ms LPBAM, `85 uA` three-full-panel-state LPBAM, `55 uA` joystick W&S disabled, and `65 uA` joystick W&S enabled; matched production-scene matrix and profile adoption remain pending |
| reactive operating point | A | workload design | energy and latency per complete event-to-yield transaction | `pending` |
| realtime operating point | A | workload design | sustained power and worst-case deadline margin | `pending` |
| package target profile | C | schema and validator | HW6 capability flags and limits linked to adopted evidence | `pending_profile_promotion`; STATE execution, package-backed masked 1bpp sprites, direct STATE replacement, four joystick cardinals, 250 ms waiting timelines, LPBAM fallback, and USB A/B install are implemented/proven inputs, but measured constants and production lifecycle gaps are not yet frozen into a shipping profile |
| rotary encoder | N/A | historical HW5 only | `input.encoder_supported = false`; `input.encoder` blocked | `not_applicable` |
| ambient light | N/A | historical HW5 only | `sensors.light_supported = false` | `not_applicable` |
| piezo / BBB | N/A | historical HW5 only | `audio.bbb_supported = false` | `not_applicable` |

## Promotion Rule

A retained HW5 result may become an HW6 result only through a new HW6 evidence
artifact. A row cannot be closed using an HW5 photo, log, GDB dump, current
capture, or user observation.

The HW6 target profile remains `pending_validation` until every capability it
grants has supporting HW6 evidence and every package-facing limit has been
adopted through [[Pending_Measured_Constants_Register]].

Related:

- [[HW6_Hardware_Revision_Contract]]
- [[HW6_Brought_Up_Tracker]]
- [[Evidence_Artifact_Convention]]
- [[Validation_Plan]]
