# EV-HW6-20260731-P5-RTOS-001

## Summary

- Test case: HW6 ThreadX owner, queue, event-group, startup-envelope, scheduler
  idle, and `PWR_DBG` heartbeat scaffold
- Result: `PASS/PARTIAL`
- Date/time: `2026-07-31 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: naked PCB; display, speaker, and housing not attached
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Firmware state: uncommitted/untracked HW6 FW0 RTOS scaffold derived from the
  base commit
- Build profile: `Debug`
- FW0 ELF SHA-256:
  `1755A7AA5BC83376B73558534489F83FCD10DAA71A2B3C51B7CCFD48C7EC246F`
- FW0 IOC SHA-256:
  `C3EA72AD444196CA2C3053D3F4F11B5462B3D4978CF9295744C341A493D90DA8`
- Instrumentation: ST-LINK SWD/GDB; target-memory snapshot from the consolidated
  `__fw0_all_probe_prints.gdb` report

## Setup

The existing bounded pre-kernel peripheral probe ran first. ThreadX then created
the documented nine owner threads, nine four-word queues, and four event-flag
groups from the generated 16 KiB byte pool. Each queue was preloaded with one
fixed startup envelope. Every owner consumed and validated its own envelope,
then blocked on a 25-tick receive timeout.

`thPower` toggled `PWR_DBG` at each 25-tick timeout. The GNU ThreadX scheduler
was built with `TX_LOW_POWER`; its idle path called the ThreadX low-power utility,
whose user entry performed normal CPU `WFI`. STOP/STOP2 and peripheral power
transitions were intentionally outside this test.

The operator allowed the target to run, halted it without reset, and sourced the
single consolidated GDB report.

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| `rtos_scaffold_gdb.log` | `hw_log` | Exact boot, peripheral, RTOS-object, queue self-test, scheduler-idle, and heartbeat snapshot |

## Results

- Existing retained-device probe remained passing: required/attempted/pass masks
  were all `0x1F`; failure mask was `0x00`.
- RTOS probe identity/version/phase was `0x48365254 / 1 / 0x66FF`.
- Initialization and runtime completion were both `1`; initialization status was
  `TX_SUCCESS`.
- Owner started and queue self-test masks both matched required mask `0x1FF`.
- Event-group self-test mask matched required mask `0x0F`.
- All stack allocation, queue allocation, queue creation, startup send, thread
  creation, event creation, event set, and event get statuses were zero.
- Every owner recorded heartbeat `45`, last tick `1100`, one startup receive,
  44 bounded timeouts, and zero message errors.
- ThreadX byte-pool availability changed from 16,376 to 5,864 bytes. The measured
  scaffold pool cost was therefore 10,512 bytes, including allocator overhead.
- Pool fragments changed from 2 to 20, consistent with 18 retained stack/queue
  allocations.
- WFI setup/enter/exit/adjust counters were `1110 / 1110 / 1109 / 1109`.
  The target was halted during the current idle entry, so entry being exactly one
  ahead of exit/adjust is expected and confirms the active scheduler idle path.
- `PWR_DBG` recorded 44 transitions through tick 1100, matching one transition
  per 25 ThreadX ticks at the configured 100 Hz tick rate.

## Conclusion

The HW6 FW0 ThreadX scaffold is valid as an initialization and scheduling
baseline. The documented owner, queue, and event-group topology fits in the
generated pool, every owner starts and blocks deterministically, every startup
message path works, and scheduler idle reaches `WFI`.

This evidence is sufficient to begin replacing startup envelopes with real
owner-routed hardware abstractions. It does not complete Phase 5 because actual
producer/consumer routing, peripheral exclusivity, queue saturation policy,
fault propagation, and power quiesce/resume barriers remain untested.

## Scope And Follow-Ups

This artifact does not validate STOP2, LPBAM, final waiting current, physical
display/audio/USB paths, gameplay runtime behavior, or package lifecycle. The
250 ms owner timeout and `PWR_DBG` toggle are diagnostic scaffold behavior and
are not the final PeepOS scheduling or debug-marker policy.
