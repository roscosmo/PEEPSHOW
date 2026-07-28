# Interface Control Document (ICD)

This document defines cross-thread and cross-layer message interfaces.

Goal: prevent hidden coupling and ad hoc payload drift.

---

## Message Design Rules

- Fixed-size structs only.
- Explicit enum for message type.
- Version field for forward compatibility.
- No pointers to temporary memory.
- No function pointers in payloads.
- Every request has ownership and timeout semantics.

---

## Queue Registry Template

For each queue define:
- queue name
- producer(s)
- consumer
- message struct
- max depth
- timeout behavior
- overflow behavior

Example:

```text
Queue: qDisplayCmd
Producers: thUI, runtime manager
Consumer: thDisplay
Payload: ps_display_cmd_t v1
Depth: KNOB_Q_DISPLAY_DEPTH
Timeout: bounded, non-infinite
Overflow: drop oldest + emit fault event
```

New active-target communication work must define `qCommCmd` before BLE implementation begins.

---

## Event Flag Registry Template

For each event group define:
- owner thread
- bit assignments
- legal setters
- legal waiters
- clear policy

Bit assignments must live in one header per event group.

---

## API Surface Boundaries

`Platform -> Engine`
- typed lifecycle, rendering, input, audio, storage, and intent APIs only

`Engine -> package/content layer`
- package metadata, assets, save APIs, and runtime-safe content APIs only

`Platform shell/services -> Engine`
- event, focus, rendering, and lifecycle abstractions only

No reverse dependency from Platform owners into Engine, package, or Reference Game modules.

---

## Change Control

Any ICD change requires:
1. schema version bump
2. docs update
3. compile-time compatibility checks
4. regression test update
