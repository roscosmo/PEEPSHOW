# RTOS Ownership and Queue Topology

This document defines thread responsibilities, queue/event topology, and ISR signaling rules.

---

## Thread Ownership Model

Recommended baseline owners:

| Thread | Owns |
|---|---|
| `thPower` | mode state, sleep class, clock transitions |
| `thDisplay` | display bus and display transfer path |
| `thAudio` | audio bus, audio DMA, amp control |
| `thInput` | raw input capture and logical action routing |
| `thUI` | shell and shared UX service flow |
| `thRuntime` | runtime host manager dispatch |
| `thStorage` | flash, filesystem, install pipeline |
| `thSensor` | sensor bus, sensor policy, health publication |
| `thComm` | BLE/NINA module, communication UART, communication policy |

---

## Queue Topology Baseline

Required queues:
- `qInputRaw` ISR -> `thInput`
- `qUIEvents` `thInput` -> `thUI`
- `qRuntimeEvents` `thInput`/`thUI` -> `thRuntime`
- `qSysEvents` multi-producer -> `thPower`
- `qDisplayCmd` multi-producer -> `thDisplay`
- `qAudioCmd` multi-producer -> `thAudio`
- `qStorageReq` multi-producer -> `thStorage`
- `qSensorReq` multi-producer -> `thSensor`
- `qCommCmd` multi-producer -> `thComm`

Exact message shapes are defined in [[Interface_Control_Document]].

---

## Event Flag Groups

Recommended groups:
- `egMode` runtime class and mode state
- `egPower` quiesce/resume coordination
- `egHealth` subsystem health/fault bits
- `egDebug` debug mode toggles

Bit ownership must be explicit and centrally defined.

---

## ISR Signaling Discipline

ISRs may:
- enqueue small raw events
- set thread flags

ISRs may not:
- perform long HAL calls
- touch filesystem/storage APIs
- change clocks/sleep states
- block or busy-wait

---

## Object Creation and Determinism

- All RTOS objects are created during init phase only.
- No runtime object creation after scheduler start.
- Thread stack sizes and queue depths are compile-time defined.
- All cross-thread waits must be bounded with explicit timeout.

---

## Forbidden Patterns

- multiple threads touching same peripheral handle
- queue payloads with transient pointers
- hidden mode changes from non-power owners
- polling loops where event-driven signaling exists

---

## Validation Evidence Required

Before runtime host feature work:
1. queue producers and consumers verified
2. mode and power event paths verified
3. quiesce/resume barrier verified
4. overflow and timeout behavior verified

Tracealyzer snapshot evidence should be used where practical to prove owner-thread scheduling, queue wake/block behavior, and quiesce/resume ordering. Snapshot capture policy is defined in [[Tracealyzer_Snapshot_Evidence_Contract]].
