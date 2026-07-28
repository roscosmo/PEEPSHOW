# Bring-Up Spec vs Brought-Up Tracker Contract

This document defines the difference between:
- the bring-up specification (what must be true)
- the brought-up tracker (what is proven so far)

Both are required.

---

## Document Roles

Bring-up specification:
- authoritative phase requirements
- expected order and pass criteria
- should change rarely

Brought-up tracker:
- live status of actual progress
- evidence ledger
- temporary measures and open gaps

Do not mix these roles in one document.

---

## Required Files

- bring-up specification: [[Bootstrap_and_Build]]
- tracker policy and statuses: this doc
- live tracker: [[Brought_Up_Tracker]]

---

## Status Tokens (Strict)

- `Not started`: no evidence logged
- `In progress`: partial evidence, open criteria remain
- `Blocked`: intentionally unavailable until prerequisite evidence or architecture work exists
- `Complete`: all criteria met and evidence logged

If any required evidence is missing, status is not complete.

---

## Tracker Update Rules

- update tracker only from measured evidence
- include date, firmware commit, board revision, and artifact link
- never mark complete based on assumption or one-off observation
- keep historical snapshots in archive section or archive file

---

## Phase Close Checklist

To close a phase:
1. all phase goals pass
2. known temporary measures are documented
3. relevant failure paths were exercised
4. evidence artifacts are linked

---

## Temporary Measures Register

Tracker must include a temporary measure table:
- measure ID
- reason
- owner
- exit criteria
- status (`active`, `scheduled_remove`, `removed`)

This prevents temporary paths from becoming permanent by accident.
