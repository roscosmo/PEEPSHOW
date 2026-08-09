# Validation Plan and Acceptance Matrix

This document defines what must be proven before platform freeze and before package-facing expansion.

---

## Validation Stages

1. Hardware bring-up validation
2. Platform service validation
3. Runtime host lifecycle validation
4. Low-power and wake validation
5. Installer/storage ownership validation
6. Long-run stability validation

---

## Minimum Test Matrix

Per mode (`SHELL`, `LP_GRAPH`, `LP_TEMPLATE`, `RT_SCENE`, `INSTALLER`):
- mode entry/exit
- owner-thread health
- display/audio/input behavior
- storage permissions enforcement
- suspend/resume behavior
- fault recovery path

---

## Power Validation Cases

- idle current at each runtime class
- wake source correctness for all enabled sources
- STOP2 residency ratio under expected workload
- repeated stop/resume cycle test (long-run)

---

## Runtime Lifecycle Cases

For each host:
1. mount success path
2. suspend during active work
3. resume after sleep wake
4. unmount and switch to another host
5. failure injection during each lifecycle phase

---

## Public vs Private Knob Validation Cases

1. private knob write attempt from package/runtime is rejected
2. public knob write in allowed mode and bounds is accepted
3. public knob write out-of-range is rejected
4. public knob write in disallowed mode is rejected
5. owner-thread routing is enforced for all applied writes
6. rejected writes emit diagnostics without destabilizing runtime

---

## Installer Cases

- local to installer handoff
- USB export with no competing storage users
- package stage/validate/commit
- safe rollback on failure
- return to shell and remount local storage

---

## Exit Criteria For Platform Freeze

Platform freeze can be approved only when:
1. all mandatory tests pass
2. no unresolved unknown wake causes remain
3. no cross-thread ownership violations remain
4. docs and implementation are in sync

---

## Evidence Requirements

Every test run record must include:
- board revision and firmware commit
- test case ID and expected behavior
- pass/fail result
- artifact links (logs, captures, traces)
- issue IDs for failures
