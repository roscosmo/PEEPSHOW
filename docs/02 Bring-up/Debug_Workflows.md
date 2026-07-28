# Debug Workflows

This document defines practical debug workflows and command discipline.

---

## Debug Interface Policy

Primary:
- SWD

Preferred runtime visibility:
- SWO structured events
- Tracealyzer snapshot/ring-buffer traces for RTOS owner behavior

Optional:
- USB CDC developer personality for structured diagnostics/control, rate-limited, disableable, and mutually exclusive with MSC in v1

No general UART printf logging policy by default.

CDC developer workflow is defined in [[USB_Development_Mode_Contract]]. Live tuning behavior is defined in [[Live_Tuning_And_Knobs_Contract]].

Tracealyzer snapshot evidence policy is defined in [[Tracealyzer_Snapshot_Evidence_Contract]].

Dashboard-facing telemetry is defined in [[Telemetry_And_Debug_Dashboard_Contract]].

Power measurement and trace correlation is defined in [[Power_Measurement_and_Trace_Correlation_Runbook]].

Repeatable host workflows should be wrapped by [[Dev_Orchestration_CLI_Contract]] once the CLI exists.

Agent-run build, flash, and debugger inspection must follow [[Bounded_Build_Flash_Debug_Runbook]].

---

## debug.gdb Contract

Project root `debug.gdb` is authoritative.

Rules:
- keep only high-value breakpoints
- keep breakpoint count bounded
- remove stale experimental helpers
- keep helper names stable
- orchestration commands may call stable helpers, but must not depend on stale experimental helpers
- agent/orchestration debug commands must not call resume-style helpers unless the run is explicitly approved and externally timed according to [[Bounded_Build_Flash_Debug_Runbook]]

---

## Required Fault Capture

On HardFault capture:
- stacked registers
- PC/LR
- CFSR/HFSR
- MMFAR/BFAR when valid
- current mode/runtime identifiers

HardFault data must be captured before reset.

---

## Bring-Up Debug Workflow

1. validate reset and early clocks
2. validate each peripheral in isolation
3. validate owner-thread message flow
4. validate sleep entry and wake traces

Prefer trace markers over frequent breakpoint halts.

## Bounded Build, Flash, And Debug Workflow

Agents may run build, flash, and debugger inspection only through bounded wrappers.

Rules:

- build wrappers must use hard timeouts, stdout/stderr capture, exit-code reporting, and leftover process checks.
- flash wrappers must verify download and include reset.
- GDB must run in batch mode through a bounded wrapper.
- safe unattended GDB inspection is limited to register/status reads and known non-resume `debug.gdb` helpers.
- `continue`, `run`, stepping commands, `_wait` helpers, and helpers that require `Ctrl-C` are not allowed unless the user explicitly approves a timed scenario.

Detailed policy is in [[Bounded_Build_Flash_Debug_Runbook]].

---

## Runtime Debug Workflow

1. capture mode/runtime state markers
2. capture subsystem state transition markers
3. correlate with evidence in brought-up tracker
4. use breakpoints only for targeted confirmation

## Dashboard Telemetry Workflow

Use the debug dashboard for structured PeepOS state, timeline, and capture review.

Allowed sources:

- SWO structured events
- CDC developer telemetry
- staged/exported capture bundles
- digital twin replay telemetry

Rules:

- dashboard telemetry is structured and schema-versioned.
- dashboard controls must use [[USB_Development_Mode_Contract]] or [[Live_Tuning_And_Knobs_Contract]].
- dashboard captures used as evidence must record source, firmware commit, knobs hash, schema versions, and artifact paths.
- dashboard telemetry does not prove physical current, wake latency, or electrical behavior.

## Tracealyzer Snapshot Workflow

Use Tracealyzer snapshots for focused RTOS and owner-thread evidence.

Recommended flow:

1. build with trace profile enabled
2. arm snapshot/ring-buffer trace
3. run one focused scenario
4. halt immediately after the scenario, marker, timeout, or fault
5. dump and save the trace artifact
6. record artifact path and trace configuration in [[Brought_Up_Tracker]]

Rules:

- snapshots prove scheduling and ordering, not electrical behavior.
- trace-enabled runs are not final current or STOP-residency evidence unless explicitly accepted by the test.
- repeated short snapshots are preferred for bring-up.
- streaming trace is future work and not required unless snapshots are insufficient.

## Power Measurement Workflow

Use PPK2 or equivalent measurement to prove current and operation energy cost.

Recommended flow:

1. choose one focused power scenario
2. enable only the markers needed for that scenario
3. use a physical sync pin if HW5 has one available, otherwise use a deliberate timed/cue alignment sequence
4. capture PPK2 current data
5. capture Tracealyzer/SWO/telemetry where useful for software context
6. record instrument setup, sync strategy, artifacts, and interpretation in evidence

Rules:

- current measurement proves electrical behavior; Tracealyzer explains software ordering.
- trace-enabled current captures are diagnostic unless confirmed by trace-disabled measurement.
- PPK2 traces and power markers are PeepOS development evidence, not game-facing APIs.

## Developer CDC Workflow

Use CDC developer mode for structured commands only.

Allowed:

- device state query
- telemetry subscription
- bounded capture start/stop/export
- package upload through firmware-owned staging
- live-safe Platform tuning through [[Live_Tuning_And_Knobs_Contract]] owner-routed requests

Disallowed:

- raw memory poking as the normal tuning path
- raw flash or filesystem access
- concurrent MSC staging/export ownership
- direct HAL, RTOS, or Platform private struct access

---

## Common Playbooks

No boot:
- connect under reset
- validate early init markers
- inspect fault record

Unexpected fault/reset:
- inspect HardFault data
- inspect last transition markers
- inspect owner health flags

Sleep not waking:
- verify wake-source classification
- inspect sleep/awake transition markers
- verify quiesce/resume acknowledgements

Installer corruption:
- verify storage ownership mode
- verify mount/export transitions
- verify single-writer behavior

---

## Evidence Logging Requirements

Each debug session must record:
- date/time
- board revision
- firmware commit
- knobs stamp/hash
- active USB personality, if USB was involved
- active live-tuning overlay, if any
- trace profile and snapshot artifact path, if tracing was enabled
- telemetry capture artifact path and schema version, if dashboard telemetry was captured
- orchestration CLI command line, if used
- test focus
- artifact paths

Log summary should be copied into the brought-up tracker evidence section.
