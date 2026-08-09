# Brought-Up Tracker Template

Use this as the live progress tracker for hardware and firmware bring-up.

---

## Metadata

- Last updated: `YYYY-MM-DD`
- Board revision: `TBD`
- Firmware commit: `TBD`
- Maintainer: `TBD`

---

## Bring-Up Phase Status

| Phase | Status | Notes |
|---|---|---|
| 0 - Power and clock stability | Not started | |
| 1 - Display validation | Not started | |
| 2 - Storage validation | Not started | |
| 3 - Audio validation | Not started | |
| 4 - Input and sensors | Not started | |
| 5 - RTOS owner integration | Not started | |
| 6 - Sleep and wake validation | Not started | |
| 7 - Installer and transport mode | Not started | |
| 8 - Runtime host lifecycle | Not started | |
| 9 - Platform freeze checks | Not started | |

---

## Evidence Ledger

For each evidence entry record:
- date/time
- test case ID
- mode/runtime class
- result
- artifacts
- notes

Template row:

| Date | Test Case | Mode/Host | Result | Artifact | Notes |
|---|---|---|---|---|---|
| YYYY-MM-DD | T-XXX | SHELL | PASS | path/to/log | |

---

## Temporary Measures Register

| ID | Introduced | Scope | Exit Criteria | Owner | Status |
|---|---|---|---|---|---|
| TMP-XXX | YYYY-MM-DD | TBD | TBD | TBD | active |

---

## Open Issues Blocking Completion

| ID | Blocking Phase | Summary | Owner | Next Action |
|---|---|---|---|---|
| BUG-XXX | Phase X | TBD | TBD | TBD |

