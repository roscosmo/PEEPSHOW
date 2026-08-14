# Debug and Observability Contract

This document defines what must be observable in bring-up and runtime without destabilizing low-power behavior.

Package-facing diagnostics are defined in [[Diagnostics_API_Contract]]. Packages do not own debug transports or persistent fault storage.

Tracealyzer snapshot evidence policy is defined in [[Tracealyzer_Snapshot_Evidence_Contract]].

Dashboard-facing telemetry is defined in [[Telemetry_And_Debug_Dashboard_Contract]].

---

## Allowed Debug Channels

- SWD (required)
- SWO structured events (preferred)
- TraceX/Tracealyzer snapshot/ring-buffer traces for RTOS and owner-thread bring-up evidence
- HW6 `PH1` / `PWR_DBG` physical timing marker for PPK2 logic correlation in development evidence builds
- USB CDC developer personality optional, rate-limited, and mutually exclusive with MSC in v1

No UART printf console policy by default.

USB CDC developer mode is defined in [[USB_Development_Mode_Contract]]. Live tuning behavior is defined in [[Live_Tuning_And_Knobs_Contract]].

Rules:

- SWO/SWV is primarily observation from device to host.
- CDC developer mode is the structured control path for live-safe Platform tuning, telemetry queries, captures, and package upload.
- CDC commands must route through owner-thread or Engine service requests.
- CDC must not expose raw memory, protected storage, HAL handles, RTOS objects, or arbitrary filesystem paths.
- release/shipping builds must disable CDC developer control unless a future policy explicitly defines a limited diagnostic subset.

## HW6 Physical Marker

`PH1` / `PWR_DBG` is a spare output routed through the HW6 battery connector
to a PPK2 logic input. Platform diagnostics owns it.

Rules:

- default and idle level is low
- marker use is development-only and enabled by an explicit evidence build or bounded diagnostic scenario
- each scenario defines whether high means a pulse, interval, state, or edge; there is no global active polarity
- the evidence manifest records pin, voltage domain, marker semantics, pulse width or interval rule, firmware commit, build profile, and instrumentation connection
- the marker returns low when the scenario completes or faults
- Engine, packages, and authoring tools cannot access or configure the pin
- normal shipping builds keep it low and compile out marker-driving paths

See [[HW6_Pin_Ownership_Matrix]] and
[[Power_Measurement_and_Trace_Correlation_Runbook]].

---

## Required Event Visibility

Must be observable with low overhead:
- mode transitions
- subsystem state transitions
- runtime lifecycle transitions
- storage ownership transitions
- fault and recovery transitions
- owner-thread scheduling and queue/event wake paths during trace-enabled bring-up builds
- dashboard-facing state vector and bounded telemetry events where a dev/debug profile enables them

---

## HW6 FW0 SWO Lifecycle Tokens

HW6 FW0 may emit sparse three-letter lifecycle tokens over SWO/ITM stimulus port 0 when `KNOB_DEBUG_SWO_LIFECYCLE_ENABLE=1` and the debugger has SWO enabled.

Rules:

- tokens are observation-only and must not change firmware control flow
- token writes must be non-blocking; if SWO is not ready, the event is dropped and counted in `g_ps_hw6_trace_probe`
- tokens mark lifecycle boundaries only, not loop progress or high-rate data
- SWO tokens complement TraceX snapshots and GDB probes; they do not replace owner-thread probes or fault records

Current HW6 FW0 token meanings:

| Token | Meaning |
| --- | --- |
| `BTD` | boot/runtime owner self-tests completed |
| `RDY` | storage entered `STORAGE_FLASH_READY` |
| `REQ` | explicit storage flash initialization requested |
| `WAK` | flash wake succeeded during explicit flash init |
| `LAY` | storage layout validation succeeded during explicit flash init |
| `ERS` | USB staging erase started |
| `FMT` | FAT/FileX format started |
| `DON` | explicit storage flash initialization completed |
| `ERR` | current lifecycle command failed before completion |
| `EXP` | USB MSC export requested |
| `MOK` | MSC media open succeeded |
| `REC` | MSC media open found invalid media and requires explicit flash init |
| `REL` | USB MSC reclaim requested |
| `RDN` | USB MSC reclaim completed |

Target GDB status helper: `firmware/peepshow_hw6_fw0/__fw0_swo_lifecycle_prints.gdb`.

### HW6 FW0 Temporary Display Lifecycle Cues

Until live SWO capture or CDC developer status is validated, HW6 FW0 bring-up may use static on-screen cues for explicit flash provisioning and USB MSC export/reclaim. These cues are observation-only: they must be sent through `thDisplay`, must not access storage or USB directly, and must not decide whether MSC export, erase, format, or reclaim happens.

Current temporary cue meanings:

| Display text | Meaning |
| --- | --- |
| `FLASH INIT / USB STAGING / WAIT` | explicit USB staging provisioning is running |
| `FLASH INIT / USB STAGING / DONE` | explicit USB staging provisioning finished successfully |
| `FLASH INIT / USB STAGING / ERROR` | explicit USB staging provisioning failed; inspect GDB probes |
| `USB MSC / EXPORT / WAIT` | MSC export command has started |
| `USB MSC / ACTIVE / EJECT FIRST` | MSC export started and the host may mount the staging volume |
| `USB MSC / RECLAIM / WAIT` | firmware is reclaiming the exported staging volume |
| `USB MSC / RECLAIM / DONE` | reclaim completed and USB hardware/clock policy returned to firmware control |
| `USB MSC / ERROR / SEE GDB` | MSC export or reclaim failed; inspect GDB probes |
| `USB MSC / MSC NEEDS / FLASH INIT` | MSC export found invalid staging media; run the explicit flash init command before retrying export |

These cues are expected to be removed or replaced once the shell/installer UI and live debug transport are mature.

### HW6 FW0 Live SWO Tooling Block

Status as of 2026-08-12: firmware-side SWO lifecycle markers exist, but live SWO capture is blocked by the current VS Code Cortex-Debug + ST-LINK GDB server backend.

Observed debugger behavior:

- Adding a Cortex-Debug `swoConfig` block to `firmware/peepshow_hw6_fw0/.vscode/launch.json` produced the warning: `SWO support is not available from the probe when using the ST-Link GDB server. Disabling SWO.`
- After boot, `firmware/peepshow_hw6_fw0/__fw0_swo_lifecycle_prints.gdb` reported `swo emit/drop/disabled = 0 / 1 / 0`, `swo last token/status = 0x445442 / 0xfffffffd`, and `swo last text = BTD`.
- That proves the firmware called the SWO marker path for `BTD`, but the debugger had not enabled ITM/SWO, so the non-blocking writer correctly dropped the token as `PS_HW6_TRACE_STATUS_NOT_READY`.

Current rule for HW6 FW0 bring-up:

- Do not keep `swoConfig` enabled in the ST-LINK GDB server launch profile; Cortex-Debug disables it anyway.
- Keep the firmware SWO scaffold because it is harmless when SWO is not ready and will work with a backend that enables ITM stimulus port 0.
- Until live SWO is validated, use GDB status helpers, TraceX snapshots, display text, and `PH1` / `PWR_DBG` for operator feedback.

Candidate solution path for a separate debug-tooling task:

1. Validate a Cortex-Debug backend that supports live SWO with ST-LINK hardware, likely OpenOCD/ST-Link rather than the ST-LINK GDB server.
2. Confirm GDB script compatibility remains intact by sourcing the existing FW0 helpers after launch.
3. Confirm reset, flash load, halt, interrupt, low-power debug, and reconnect behavior are not worse than the current ST-LINK GDB server flow.
4. Confirm live text tokens `BTD` and `RDY` appear without pausing the target.
5. Confirm a destructive flash provisioning run shows the expected token sequence ending in `DON` or `ERR` without requiring a mid-operation GDB interrupt.
6. Only after that validation should the project launch profile grow a live-SWO variant.

This is a tooling limitation, not evidence of a firmware lifecycle failure.

---

## TraceX Runtime Scaffold

TraceX is allowed for HW6 FW0 bring-up as a bounded, static RAM trace buffer. It is an observation tool for RTOS scheduling, object creation, event flags, queue activity, and owner-thread lifecycle behavior. It is not a package-facing diagnostic API and it must not become a hidden control path.

Rules:

- TraceX compile/runtime enable is development-only and must be controlled by Platform debug knobs.
- The trace buffer is statically allocated; no heap allocation is allowed.
- The runtime enable call must happen after ThreadX internal trace initialization and before Platform-owned ThreadX objects are created.
- The trace buffer size and object registry count are compile-time knobs, because they change RAM budget and capture depth.
- TraceX capture must not replace explicit owner-state probes, PMIC snapshots, fault records, or USB/MSC evidence.
- STOP2 evidence must state whether TraceX/SWD/debug-in-low-power changed the behavior being measured.

HW6 FW0 validated baseline:

- CubeMX generated Trace Async/SWO and `TX_ENABLE_EVENT_TRACE` support.
- FW0 calls `tx_trace_enable()` from the `tx_application_define()` user block in `AZURE_RTOS/App/app_azure_rtos.c`.
- Current knobs: `KNOB_DEBUG_TRACEX_ENABLE=1`, `KNOB_DEBUG_TRACEX_BUFFER_BYTES=32768`, `KNOB_DEBUG_TRACEX_REGISTRY_ENTRIES=64`, `KNOB_DEBUG_TRACEX_USER_EVENTS_ENABLE=1`, `KNOB_DEBUG_SWO_LIFECYCLE_ENABLE=1`.
- Target GDB status helper: `firmware/peepshow_hw6_fw0/__fw0_tracex_prints.gdb`.
- Target GDB dump helper: `firmware/peepshow_hw6_fw0/__fw0_tracex_dump.gdb`; while halted, it writes the live TraceX buffer to latest snapshot `firmware/peepshow_hw6_fw0/TraceFiles/__fw0_tracex_snapshot.trx` and asks the host shell to copy that dump to a timestamped sibling named `__fw0_tracex_snapshot_YYYYMMDD_HHMMSS.trx`.
- First target status evidence returned `enable status/runtime = 0x0 / 1`, buffer `0x2000a7a0 / 32768`, trace start/end `0x2000b3d0 / 0x20012790`, and registry total/available `64 / 29`.
- First target dump evidence wrote `firmware/peepshow_hw6_fw0/TraceFiles/__fw0_tracex_snapshot.trx` at `32768` bytes. TraceX viewer import was validated by user-provided screenshot showing Azure RTOS TraceX 6.4.0 opening the snapshot with named ThreadX/owner threads and scheduler activity.
- PeepShow app marker validation returned `trace api/count ok/skip/err = 1 / 35 / 35 / 0 / 0`, with last event `0x5110` and last status `0x0`, proving the FW0 user-event wrapper inserted owner/UI/input/power markers successfully during a menu interaction trace.

---

## PMIC Interrupt Observability

PMIC interrupt evidence must show both the MCU edge and the power-owner work it
caused. An ISR counter by itself only proves the MCU saw an edge; it does not
prove PMIC policy ran. Required PMIC_INT evidence includes:

- EXTI edge count and last edge timestamp
- pending/consumed count showing the event reached `thPower`
- normal ADP5360 snapshot status from `thPower` context
- interrupt enable registers and read statuses
- interrupt flag pre-clear values and write-one-clear statuses
- confirmation that VBUS/charger events did not trigger USB MSC export or
  storage ownership by themselves

On HW6 FW0, `EXTI15` is deliberately disarmed during early startup and armed
only after RTOS owner services exist. This prevents early PMIC_INT edges from
running owner-dependent work before ThreadX initialization is complete.

---

## HardFault Record Contract

On HardFault capture:
- PC, LR
- stacked registers
- CFSR, HFSR
- MMFAR, BFAR when valid
- current mode/runtime state identifiers

HardFault data must be captured before reset.

---

## Breakpoint Policy

- avoid high-frequency ISRs and DMA callbacks
- cap active breakpoints during runtime debugging
- prefer event markers over stop-heavy breakpoint sessions for STOP validation

---

## STOP2 Debug Policy

- enable debug-in-low-power when investigating STOP behavior, but do not treat that as physical sleep/wake proof
- classify every wake source with evidence
- avoid instrumentation that materially changes sleep behavior
- STOP2 evidence must record `DBGMCU->CR` before/after entry; on STM32U575 the debug MCU configuration register is at `0xE0044004`, not `0xE0044000`
- controlled physical START-wake proof on HW6 FW0 used `firmware/peepshow_hw6_fw0/__fw0_stop2_debug_low_power_off.gdb` to clear `DBG_STOP` and `DBG_STANDBY`, then restored them only when debug visibility was needed again

HW6 FW0 evidence `EV-HW6-20260814-P1-STOP2WAKE-056` showed why this distinction matters: with `DBGMCU->CR = 0x6`, the controlled STOP2 helper entered and returned immediately with no EXTI, GPIO, NVIC, PMIC, or PWR wake-source evidence, so the result was classified as `UNKNOWN`. After clearing `DBGMCU->CR` to `0x0`, the same controlled helper entered STOP2 and woke from physical START with source mask `0x1`, primary cause `START`, PA4 IDR changing `0x6055 -> 0x6045`, START button edges `0 -> 1`, `PWR_SR.STOPF` set, and no unknown wake count.
---

## Production Build Guardrails

Release builds must:
- compile out verbose debug paths
- keep structured fault capture
- preserve deterministic timing behavior


