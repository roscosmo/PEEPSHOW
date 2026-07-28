# HW6 Revalidation Matrix

This matrix defines which HW5 results may guide HW6 testing and what must be
remeasured before the active HW6 target profile is published.

Status: `pre_arrival`

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
| board identity and assembly intake | A | checklist structure only | photos, revision IDs, board ID, assembly lot, rework state | `pending` |
| unpowered shorts and continuity | A | expected-net guide | measured resistance/continuity on received unit | `pending` |
| ADP5360 rails and faults | A | register sequence hypothesis | first-power current, 1.8/3.3 V rails, PGOOD/fault, safe configuration | `pending` |
| reset, BOOT0, SWD recovery | A | recovery procedure | attach-under-reset, ID, halt/reset, safe flash recovery | `pending` |
| clock tree and operating points | A | candidate points only | generated register audit, clock outputs/derived timings, reactive/realtime sweeps | `pending` |
| hardwired display translator | A | HW5 `PD2 VLT_LCD` control behavior is not transferable | confirm translator is continuously enabled and EXTCOMIN reaches the powered panel in active and STOP contexts | `pending` |
| static display transfer | B | expected mapping and patterns | HW6 polarity, row order, byte order, full/partial transfer | `pending` |
| LPDMA and SRAM4 | A | sizing hypothesis | HW6 map report, reachability, alignment, maximum reliable transfer | `pending` |
| LPBAM autonomous display | A | validated experiment design | STOP2 playback, cadence, seams, wake/abort, seeded handoff, current | `pending` |
| external flash | B | command baseline | JEDEC/status, scratch erase/program/readback, deep-power-down | `pending` |
| speaker audio | B | sample/DMA baseline | 16 kHz clock, SAI DMA, audible output, current, clean shutdown | `pending` |
| buttons and BOOT0 | A | logical mapping baseline | levels, EXTI, debounce, wake, lock/unlock consumption, boot boundary | `pending` |
| joystick / TMAG3001 | B | driver and threshold baseline | identity, axis mapping, threshold IRQ, sleep current | `pending` |
| IMU / LIS2DUX12 | B | driver and sleep baseline | identity, events, lowest-power mode, interrupt, current | `pending` |
| NINA UART and flow control | B | command sequence baseline | boot, identity, RTS/CTS, command/data mode, sleep/wake | `pending` |
| BLE and NFC | B | HW5 experiment procedure | advertising, pairing/session, SPS TX/RX, NFC read, current | `pending` |
| USB MSC / storage ownership | B | recovery procedure | attach/enumeration, mount/reclaim, disconnect recovery, power | `pending` |
| RTOS owner topology | B | code/queue design | owner creation, queue routing, quiesce acknowledgements, fault paths | `pending` |
| STOP2 and wake matrix | A | expected sequence | every HW6 wake source, unknown wake handling, resume, repeated cycles | `pending` |
| waiting current | A | approximate goal only | instrumentation-minimized HW6 current by backend/context | `pending` |
| reactive operating point | A | workload design | energy and latency per complete event-to-yield transaction | `pending` |
| realtime operating point | A | workload design | sustained power and worst-case deadline margin | `pending` |
| package target profile | C | schema and validator | HW6 capability flags and limits linked to adopted evidence | `pending` |
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
