# Knobs and Compile-Time Tuning Contract

This document defines the knobs system contract for all firmware tunables.

---

## Visibility Model (Authoritative)

All knobs are classified as exactly one visibility:
- `private`: firmware-only; never exposed to package or game APIs.
- `public`: safe, bounded knobs that may be requested through package/runtime APIs.

Rules:
- every knob must declare `visibility`
- default visibility is `private`
- `critical` or hardware-safety knobs must always remain `private`

## Pipeline (Authoritative)

```
config/knobs.json
  -> tools/gen_knobs.py
  -> Core/Inc/knobs_private_autogen.h
  -> Core/Inc/public_knobs_ids.h
  -> package_sdk/public_knobs_contract.json
```

Rules:
- firmware includes generated private header internally
- package/runtime layer uses generated public contract and IDs only
- firmware never parses JSON at runtime
- knob changes require regenerate plus rebuild

---

## Required Files

- `config/knobs.json` source of truth
- `config/knobs.schema.json` validation and editor metadata
- `tools/gen_knobs.py` generator
- `Core/Inc/knobs_private_autogen.h` generated private output
- `Core/Inc/public_knobs_ids.h` generated public knob IDs
- `package_sdk/public_knobs_contract.json` generated public knob contract

Generated outputs are never manually edited.

---

## Required Schema Fields

Each knob schema entry must declare:
- `visibility`: `public|private`
- `owner`: `power|display|audio|sensor|storage|runtime|ui`
- `risk`: `safe|sensitive|critical`
- `type`, bounds, and default

If `visibility=public`, schema must also declare:
- `modes_allowed` (runtime classes allowed to request changes)
- clear min/max or enum bounds
- deterministic behavior notes

Validation rules:
- `public` knobs must have `risk=safe`
- `critical` knobs must not be `public`
- missing `owner` or bounds is schema-invalid

---

## Timebase Domain Contract

All timing knobs must declare:
- authored domain
- runtime compare domain

Allowed domains:
- `threadx`
- `hal_ms`
- `knob_rtos_tick_hz`

Any conversion must happen once at an explicit boundary.

---

## Adding a New Knob

1. add key to `config/knobs.json`
2. add schema entry with visibility, owner, risk, and constraints
3. regenerate outputs
4. consume generated private macro in firmware owner code
5. if public, consume generated ID/contract through public API layer
6. document effect, ownership, and allowed runtime classes

---

## Runtime Access Contract

Public knob requests must flow through one API boundary:
- `ps_public_knob_set(id, value)`
- `ps_public_knob_get(id, out_value)` (optional read path)

Request handling must enforce, in order:
1. knob exists and is `public`
2. requesting runtime class is in `modes_allowed`
3. value is within allowed bounds
4. request is routed to the owning thread
5. owner thread applies and confirms result

Packages and runtimes must never access private knobs directly.

---

## Knob Hygiene Rules

- no duplicated tunables for same behavior
- no stale/unused knob keys
- names must include subsystem context
- tuning knobs must not bypass ownership boundaries
- public knobs must have explicit safety rationale
- private knobs must not leak into package SDK artifacts

---

## Determinism Rules

Knobs must not:
- add random timing behavior
- introduce unbounded retries
- silently change architectural contracts
- bypass owner-thread arbitration

---

## Safety Rails

- owner thread re-validates and clamps applied public values
- forbidden mode or ownership violations are rejected and logged
- safety-critical hardware limits are hardcoded private limits, not public knobs

---

## CI and Lint Requirements

Build must fail if:
- a `public` knob is missing bounds, owner, or modes
- a `critical` knob is marked `public`
- generated public contract is out of sync with schema
- package SDK references unknown knob IDs

---

## Version and Traceability

Track a knob-set stamp for each bring-up and validation run:
- firmware commit
- knobs hash or version
- board revision

Store stamps in validation evidence records.
