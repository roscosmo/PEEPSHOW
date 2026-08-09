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

`platform -> runtime_hosts`
- typed host lifecycle APIs only

`runtime_hosts -> package layer`
- package metadata/assets/save APIs only
- package-visible public knob APIs only

`ui_services -> runtime_hosts`
- event and rendering abstractions only

No reverse dependency from platform owners into package-specific modules.

---

## Public Knob Message Contract

Define one canonical request/response path for public knobs.

Request payload requirements:
- `knob_id`
- requested value
- requester runtime class
- requester package/session ID (if applicable)
- request token for correlation

Response payload requirements:
- request token
- result code (`OK`, `ERR_NOT_PUBLIC`, `ERR_OUT_OF_RANGE`, `ERR_MODE_BLOCKED`, `ERR_OWNER_REJECTED`)
- applied value when successful

Routing rule:
- validation happens once in public-knob service boundary
- apply happens in owner thread only

---

## Change Control

Any ICD change requires:
1. schema version bump
2. docs update
3. compile-time compatibility checks
4. regression test update
