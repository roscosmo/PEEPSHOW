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
- Current knobs: `KNOB_DEBUG_TRACEX_ENABLE=1`, `KNOB_DEBUG_TRACEX_BUFFER_BYTES=32768`, `KNOB_DEBUG_TRACEX_REGISTRY_ENTRIES=64`.
- Target GDB helper: `firmware/peepshow_hw6_fw0/__fw0_tracex_prints.gdb`.
- First target evidence returned `enable status/runtime = 0x0 / 1`, buffer `0x2000a7a0 / 32768`, trace start/end `0x2000b3d0 / 0x20012790`, and registry total/available `64 / 29`.

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

- enable debug-in-low-power when investigating STOP behavior
- classify every wake source with evidence
- avoid instrumentation that materially changes sleep behavior

---

## Production Build Guardrails

Release builds must:
- compile out verbose debug paths
- keep structured fault capture
- preserve deterministic timing behavior

