# Evidence Artifact Convention

This document defines naming, storage, and metadata conventions for bring-up and validation evidence artifacts.

Evidence proves behavior only when it is linked from the tracker for the physical target under test and includes enough metadata to reproduce the test context. [[Brought_Up_Tracker]] routes to the active and retired target-specific ledgers.

Related:

- [[Brought_Up_Tracker]]
- [[Validation_Plan]]
- [[Debug_Workflows]]
- [[Power_Measurement_and_Trace_Correlation_Runbook]]
- [[Tracealyzer_Snapshot_Evidence_Contract]]
- [[Telemetry_And_Debug_Dashboard_Contract]]
- [[Dev_Orchestration_CLI_Contract]]
- [[Knobs_and_Tuning_Contract]]

---

## Purpose

The evidence convention prevents bring-up artifacts from becoming untraceable.

It standardizes:

- evidence IDs
- artifact paths
- capture metadata
- trace/dashboard/current-log naming
- link format from the tracker
- distinction between hardware evidence, host evidence, and design-time output

---

## Evidence Classes

| Class | Meaning | Can Prove Hardware? |
|---|---|---|
| `hw_measurement` | measured on an identified physical target with a physical instrument or firmware capture | yes, for that identified target |
| `hw_trace` | Tracealyzer/SWO/telemetry from firmware running on an identified physical target | yes for scheduling/state evidence, not electrical behavior |
| `hw_log` | firmware/dev-tool log from an identified physical target | yes where the test case allows |
| `host_twin` | digital twin replay/capture | no |
| `host_tool` | package compiler, validator, asset pipeline, dashboard decode | no |
| `design_doc` | schematic/doc/spec extract | no by itself |
| `photo_video` | physical photo/video evidence | yes only for visible/mechanical/display behavior |

Rules:

- digital twin artifacts must never be recorded as hardware bring-up evidence.
- Tracealyzer snapshots can prove owner scheduling and state sequencing, not current draw.
- current, voltage, wake latency, and sleep behavior need physical or HW firmware measurement artifacts.

---

## Evidence ID Format

Use stable, target-qualified evidence IDs:

```text
EV-TARGET-YYYYMMDD-PHASE-SUBSYSTEM-NNN
```

Examples:

```text
EV-HW6-20260731-P0-POWER-001
EV-HW6-20260801-P1-DISPLAY-002
EV-HW6-20260802-P6-SLEEP-001
```

Rules:

- `TARGET` identifies the physical hardware target, for example `HW6`.
- `PHASE` matches the bring-up phase where practical, for example `P1`.
- `SUBSYSTEM` uses an uppercase short name, for example `POWER`, `DISPLAY`, `STORAGE`, `AUDIO`, `INPUT`, `SENSOR`, `BLE`, `SLEEP`, `USB`, `RTOS`, `TWIN`.
- evidence IDs are not reused.
- failed tests still get evidence IDs when artifacts are useful.
- existing HW5 artifacts using the legacy `EV-YYYYMMDD-PHASE-SUBSYSTEM-NNN` form remain valid historical evidence; do not rename them solely to match the new format.

---

## Artifact Path Convention

Preferred path shape:

```text
docs/02 Bring-up/Evidence/TARGET/YYYY/MM/DD/EV-TARGET-YYYYMMDD-PHASE-SUBSYSTEM-NNN/
```

Each evidence folder should contain:

```text
manifest.md
artifacts...
```

Example:

```text
docs/02 Bring-up/Evidence/HW6/2026/08/01/EV-HW6-20260801-P1-DISPLAY-002/
  manifest.md
  display_pattern_photo.jpg
  telemetry.jsonl
  trace.psfs
  notes.md
```

Rules:

- artifact filenames should be descriptive and stable.
- raw exported artifacts should be preserved where practical.
- derived screenshots/plots should identify the source artifact.
- do not store secrets, host usernames, raw private filesystem paths, or protected storage dumps in evidence artifacts.

---

## Manifest Template

Each evidence folder should include `manifest.md`.

```text
# EV-TARGET-YYYYMMDD-PHASE-SUBSYSTEM-NNN

## Summary

- Test case:
- Result:
- Date/time:
- Maintainer:
- Hardware target:
- Board revision:
- Board ID/serial:
- Assembly source/lot:
- Hardware rework state:
- Firmware commit:
- Build profile:
- Target profile:
- Platform contract revision:
- Knobs hash/version:
- Active tuning overlay:
- Instrumentation:

## Setup

- Hardware:
- Instruments:
- Host OS/tool versions:
- USB personality:
- Trace profile:
- Telemetry schema version:

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| file.ext | hw_measurement | |

## Observations

## Conclusion

## Follow-Ups
```

Rules:

- `Result` must be `PASS`, `FAIL`, `PARTIAL`, `BLOCKED`, or `INFO`.
- `Active tuning overlay` must be `none` or list the overlay artifact/path.
- `Instrumentation` must note Tracealyzer, SWO, CDC telemetry, dashboard, current probe, logic analyzer, oscilloscope, or none.
- conclusions must not mark behavior known-good unless the artifact class can prove it.

---

## Tracker Link Format

Target-specific tracker rows should reference evidence IDs and paths. For active HW6 work, use [[HW6_Brought_Up_Tracker]].

Example:

| Date | Test Case | Mode/Host | Result | Artifact | Notes |
|---|---|---|---|---|---|
| 2026-08-01 | P1-DISPLAY-PATTERN | HW6 | PASS | `EV-HW6-20260801-P1-DISPLAY-002` | logical/native mapping verified |

Rules:

- tracker rows link to evidence folders, not loose screenshots where practical.
- tracker notes summarize the result, not the full evidence.
- detailed setup and raw artifacts live in the evidence manifest/folder.

---

## Required Artifact Metadata By Type

| Artifact Type | Required Metadata |
|---|---|
| Tracealyzer snapshot | trace profile, buffer size, firmware commit, thread names, capture window |
| SWO/telemetry log | schema version, source, firmware commit, timestamp basis |
| dashboard capture | dashboard version, telemetry schema, source type, profile |
| current/power log | instrument model, sample rate, shunt/range, calibration notes, source voltage/current limit, sync marker strategy |
| logic analyzer/oscilloscope | instrument, channel mapping, sample rate, trigger condition |
| display photo/video | pattern name, expected output, lighting notes where relevant |
| package validation output | package hash, target profile, validator version |
| digital twin replay | package hash, twin profile, input/time/sensor trace, content parameter overrides |

---

## Evidence Boundary Rules

- hardware evidence must identify the hardware target, board revision, board ID/serial where available, assembly/rework state, and firmware commit.
- evidence measured on one hardware target does not grant behavior to another target.
- profile-derived evidence must identify target profile and profile version.
- all evidence using live tuning must record active Platform knob overlay.
- all evidence using content parameter overrides must record package parameter hash/override file.
- failed tests must preserve enough information to reproduce the failure.
- evidence should distinguish measured data, interpretation, and follow-up actions.

---

## Validation Cases

1. evidence folder contains `manifest.md`.
2. manifest records hardware target, board identity/rework state, firmware commit, knobs hash/version, and active instrumentation.
3. the target-specific tracker row links to the evidence ID.
4. digital twin evidence is labeled `host_twin` and not used as hardware proof.
5. Tracealyzer evidence records trace profile and capture window.
6. current measurement evidence records instrument and sample configuration.

---

## Rule

If behavior is important enough to mark known-good, it needs an evidence ID, a manifest, and a tracker link.
