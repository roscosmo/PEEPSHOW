# Architecture Decisions (ADR Process)

This document defines how architecture choices are recorded so project boundaries stay stable.

---

## Why This Exists

Without a decision log, major boundary decisions can drift over time.
An ADR process prevents silent changes to core assumptions.

---

## When An ADR Is Required

Create an ADR when changing:
- layer boundaries
- ownership model
- runtime host lifecycle contract
- package schema compatibility
- power policy behavior
- memory layout or retained-state format

---

## ADR Template

Each ADR file should include:
- `Status` (`proposed`, `accepted`, `superseded`, `rejected`)
- `Date`
- `Decision`
- `Context`
- `Alternatives considered`
- `Consequences`
- `Validation plan`

Store ADR files under:
- `docs/adr/ADR-XXXX-title.md`

---

## Review Rules

- ADR approval required before related implementation starts.
- If implementation changes and ADR is stale, update ADR first.
- Superseded ADRs remain in history and link to replacements.

---

## Initial ADRs To Write Immediately

1. Runtime host classes and lifecycle.
2. Package schema versioning strategy.
3. Storage ownership and installer isolation model.
4. Wake classification and sleep policy matrix.
5. Retained state scope and validity rules.
