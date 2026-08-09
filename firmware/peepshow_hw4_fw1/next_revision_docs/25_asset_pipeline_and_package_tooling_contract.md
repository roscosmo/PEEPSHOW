# Asset Pipeline and Package Tooling Contract

This document defines how external tools produce package content for runtime hosts.

---

## Goal

Keep tooling output stable and host-oriented so tools never depend on RTOS or hardware internals.

---

## Package Build Inputs

Tooling inputs may include:
- package manifest JSON
- state graph JSON/GraphML
- asset metadata tables
- image/audio source assets
- optional script/config data allowed by host contract

---

## Package Build Outputs

Tooling outputs must include:
- normalized manifest
- packaged asset blobs/chunks
- integrity metadata
- version identifiers

Output format must be deterministic from identical inputs.

---

## Schema Governance

- all package schemas are versioned
- breaking schema changes require major version increment
- tooling must validate schema before package generation
- firmware must reject incompatible schema versions cleanly

---

## Runtime Class Compatibility

Packages must declare target runtime class.
Tooling must validate declared class against available host capabilities:
- `LP_GRAPH`
- `LP_TEMPLATE`
- `RT_SCENE`

---

## Public Knob Contract For Tooling

Tooling may reference only generated `public` knob contract artifacts.

Rules:
- tooling must never consume or expose private knob definitions
- package manifests may reference only valid public knob IDs
- tooling must validate requested defaults/overrides against public bounds and allowed modes
- invalid public-knob references fail package build

---

## Deterministic Build Rules

- no hidden timestamps in package payload unless explicitly declared
- stable ordering for generated tables/indexes
- reproducible checksums for identical inputs

---

## Validation Steps (Tool Side)

1. schema validation
2. asset bounds and format validation
3. manifest consistency checks
4. integrity/checksum generation
5. final package compatibility report

---

## Integration Rules

Tooling does not:
- emit hardware register assumptions
- embed RTOS queue assumptions
- assume specific peripheral timing implementation

Tooling does:
- target runtime/package contracts only
- emit intent and structured content

---

## Required Artifacts

For each package build retain:
- input manifest and schema versions
- tool version
- generated package checksum
- compatibility report
