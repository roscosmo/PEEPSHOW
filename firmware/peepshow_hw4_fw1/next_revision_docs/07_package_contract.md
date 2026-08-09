# Package Contract

This document defines the package-facing contract independent of hardware implementation details.

---

## Package Model

Packages provide:
- metadata
- assets
- state graphs/tables
- variables and configuration
- optional host-allowed scripted logic
- public knob preferences/requests within runtime contract limits

Packages do not provide:
- direct peripheral control
- thread creation
- power mode transitions
- private knob access

---

## Manifest Requirements

Every package must declare:
- `package_id`
- `name`
- `version`
- `runtime_class` (`LP_GRAPH`, `LP_TEMPLATE`, `RT_SCENE`)
- `required_capabilities`
- `wake_intents`
- `cadence_hints`
- `asset_table`
- `save_schema_version`

Optional manifest section:
- `public_knob_defaults` (must reference valid public knob IDs only)

---

## Intent-Driven Policy

Packages may declare intent such as:
- "wake on button and step"
- "no periodic tick needed"
- "update every 1000 ms"
- "requires short audio cues only"

Platform decides exact hardware behavior.

Packages may also request safe public knob values through host APIs.
Platform validates and applies these through owner threads only.

---

## Storage and Save Rules

- Package saves go through package storage APIs only.
- Save schema version must support migration handlers.
- Package writes must be bounded and power-safe.
- Package data cannot bypass installer validation path.

---

## Versioning and Compatibility

Use semantic versioning for package format:
- `pkg_format_major`
- `pkg_format_minor`

Rules:
- Major mismatch: reject install.
- Minor mismatch: allow if backward-compatible.
- Validation output must include exact rejection reason.

---

## Validation Checklist

At install time:
1. validate manifest schema and signatures/checksums
2. validate runtime class compatibility
3. validate asset table bounds
4. validate save schema declaration
5. stage package before commit
6. validate all referenced public knob IDs and bounds

---

## Minimum Package API Surface

Expose package-safe APIs only:
- metadata query
- asset read by ID
- save read/write by key/schema
- capability query
- host event submission
- public knob query/set (validated path only)

No HAL or RTOS internals are exposed to packages.
