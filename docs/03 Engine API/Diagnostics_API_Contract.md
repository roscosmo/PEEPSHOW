# Diagnostics API Contract

This document defines the Engine-facing diagnostics API for PeepOS packages, tools, and the digital twin.

The diagnostics API exposes bounded package/runtime observability. It does not expose SWD, SWO, UART, USB CDC, persistent fault-log storage, protected flash regions, HardFault registers, RTOS internals, peripheral driver logs, or debug transport ownership.

Related:

- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Package_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Telemetry_And_Debug_Dashboard_Contract]]
- [[Debug_and_Observability]]
- [[Boot_and_Fault_Supervisor_State_Machine]]
- [[Storage_and_Installer_Contract]]
- [[Validation_Plan]]

---

## Purpose

Package diagnostics explain package behavior.

Platform diagnostics explain device behavior.

Packages may emit bounded markers, counters, timing hints, package assertions, and package fault codes. They may not own debug transports or write diagnostic storage directly.

Minimal shipping diagnostics are allowed and useful for field diagnosis. Verbose package traces are development, authoring-preview, or digital-twin features.

---

## Ownership Boundary

The Platform owns:

- SWD/SWO/USB/debug transport policy
- persistent fault log ring
- HardFault and reset evidence
- protected diagnostic storage
- subsystem health and fault classification
- bring-up evidence capture
- debug export staging policy

The Engine owns:

- package diagnostic API admission
- diagnostic schema validation
- package fault code routing
- rate limiting and payload bounds
- build-profile gating
- digital twin diagnostic timeline
- compatibility and validation reports

Packages own:

- package-level markers
- package counters
- package timing scopes
- package assertions
- package fault codes
- diagnostic labels for authored systems

Packages do not own debug output channels.

Dashboard-facing telemetry may include package diagnostics only after Engine validation, rate limiting, and profile gating.

---

## Core Rules

- Package diagnostics are bounded data, not arbitrary logging.
- Package diagnostics must not include raw pointers, memory dumps, filesystem paths, protected storage addresses, hardware registers, RTOS object addresses, or driver state.
- Diagnostic payloads must be schema-bounded and rate-limited.
- Shipping packages may emit minimal fault evidence and selected bounded counters.
- Verbose markers, timing scopes, and trace values are gated to dev, preview, or digital twin profiles unless explicitly allowed by release policy.
- Platform faults are not package diagnostics.
- Package faults are not hardware bring-up evidence.
- Diagnostics must not materially change low-power behavior in shipping profiles.

---

## Diagnostic Levels

| Level | Purpose | Typical Profile |
|---|---|---|
| `marker` | timeline breadcrumb | dev/twin, limited release if allowed |
| `counter` | bounded numeric state | dev/twin, limited shipping |
| `timing` | package/runtime budget measurement | dev/twin |
| `trace_value` | bounded structured package value | dev/twin |
| `warning` | recoverable package/tool issue | dev/twin/report |
| `package_fault` | package logic fault requiring lifecycle handling | all profiles |

Diagnostic levels are package-facing. Platform may map them to internal traces, reports, or exports according to policy.

---

## Package-Facing API

Conceptual operations:

```text
diag.marker(marker_id)
diag.counter(counter_id, value)
diag.timing_begin(scope_id)
diag.timing_end(scope_id)
diag.trace_value(value_id, value)
diag.warning(code, context_id)
diag.package_fault(code, context_id)
diag.assert_state(condition, code, context_id)
```

Rules:

- IDs are package-local symbolic IDs compiled into bounded tables.
- values use fixed schema types.
- context payloads are optional and bounded.
- strings are compiled/static table references, not arbitrary runtime strings.
- timing scopes must be balanced or automatically closed by Engine policy.
- package faults route through Engine lifecycle policy.

---

## Diagnostic Profile Schema

Packages declare diagnostics as package data.

Conceptual schema:

```text
diagnostics_profile:
  marker_table[]
  counter_table[]
  timing_scope_table[]
  trace_value_table[]
  warning_code_table[]
  package_fault_code_table[]
  profile_gates
  rate_limits
  export_policy
```

Rules:

- each diagnostic ID must be stable within the package.
- each payload type must be declared.
- package fault codes must map to lifecycle policy.
- shipping diagnostics must be explicitly marked as shipping-allowed.
- diagnostic export policy is advisory; Platform owns actual export channels.

---

## Build Profile Gating

| Build Profile | Diagnostic Behavior |
|---|---|
| `authoring_preview` | verbose markers, counters, timing, trace values, warnings, and package faults allowed within bounds |
| `dev_package` | useful markers/counters/timing allowed within bounds |
| `release_candidate` | limited diagnostics; warnings must be resolved or waived |
| `shipping` | minimal package fault codes, package ID/runtime unit, selected bounded counters, and compatibility references only |

Shipping diagnostics must be low-overhead, deterministic, and rate-limited.

---

## Package Faults

Package faults represent package/runtime logic failures, not hardware faults.

Examples:

- state graph reached invalid node
- required package asset ID missing after validation
- package variable out of declared range
- save schema mismatch in package-owned record
- package action exceeded declared budget
- package message failed schema validation

Package faults may cause:

- recover in-place
- route to declared fallback unit
- safe stop and return to shell
- quarantine package until reinstall/update

Platform faults such as I2C failure, DMA underrun, BLE module timeout, HardFault registers, or flash program failure are Platform diagnostics and must not be emitted by package code.

---

## Export And Storage Rules

Packages do not write diagnostic files.

Rules:

- persistent fault logs remain Platform-owned protected storage.
- debug export is a Platform/storage-owner operation.
- packages may contribute bounded diagnostic records through Engine APIs.
- exported diagnostic summaries may include package diagnostic records if policy allows.
- host-visible staging/export must not expose protected diagnostic storage directly.

---

## Digital Twin Requirements

The digital twin must use the same diagnostics contract as package runtime.

Twin outputs may include:

- package marker timeline
- counter history
- timing scope summaries
- trace values
- package fault code
- final state vector
- compatibility report reference
- validation report reference

Rules:

- twin diagnostics are deterministic for a fixed trace.
- twin diagnostics may be more verbose than shipping hardware diagnostics.
- twin diagnostics are not hardware bring-up evidence.
- fault injection diagnostics explain contract-level behavior, not hardware fault physics.

---

## Tool-Time Validation

Tooling must validate diagnostics before package compilation/export.

Reject:

- arbitrary runtime string logging.
- raw pointers, memory dumps, or byte dumps.
- hardware register, pin, peripheral, RTOS object, storage address, or filesystem path diagnostics.
- unbounded marker/counter/timing/trace tables.
- unbounded diagnostic rate.
- shipping-enabled verbose diagnostics outside release policy.
- package fault code with no lifecycle route.
- diagnostic export request that assumes ownership of USB, FAT/FileX, SWD, SWO, UART, BLE, or protected storage.

Authoring tools should explain failures in PeepOS terms, such as:

```text
Diagnostic marker table has 600 entries; this profile allows 128.
```

---

## Validation Cases

1. package marker emits a bounded timeline record in dev/twin profile.
2. package counter emits fixed-schema numeric value within rate limits.
3. package timing scope reports bounded package/runtime timing without hardware callbacks.
4. shipping profile preserves minimal package fault code and runtime unit evidence.
5. verbose trace values are rejected or stripped in shipping profile unless release policy allows them.
6. package diagnostics cannot reference SWD, SWO, UART, USB, BLE, storage regions, hardware registers, or RTOS objects.
7. package fault routes through Engine lifecycle policy.
8. Platform fault remains Platform diagnostic and is not exposed as normal package diagnostic API.
9. digital twin replay produces deterministic diagnostics for a fixed trace.
10. exported diagnostic summary copies bounded package records without exposing protected storage directly.
