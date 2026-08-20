# Memory and Budgeting Contract

This document defines memory budgets and placement rules to keep behavior deterministic and low risk.

---

## Goals

- deterministic memory use
- explicit stack and queue budgets
- controlled retained-RAM usage
- no hidden dynamic allocation

---

## SRAM Budget Model

Define and maintain:
- per-thread stack budget
- per-queue storage budget
- per-subsystem working buffer budget
- retained memory budget
- package/runtime working budget
- trace/telemetry budget
- static asset staging budget

All budgets must be checked into source control.

---

## Budget Ledger Schema

Memory budgets should be recorded in a generated or maintained ledger.

Conceptual schema:

```text
memory_budget:
  budget_id
  budget_version
  board_revision
  firmware_commit
  build_profile
  target_profile
  linker_script_ref
  map_file_ref
  knobs_hash
  sections[]
  owners[]
  margins[]
  evidence_refs[]
```

Section record:

```text
section_budget:
  memory_region
  linker_section
  owner
  purpose
  budget_bytes
  measured_bytes
  headroom_bytes
  enforcement
```

Owner record:

```text
owner_budget:
  owner
  stacks_bytes
  queues_bytes
  static_buffers_bytes
  retained_bytes
  dma_safe_bytes
  trace_bytes
  notes
```

Rules:

- every owner thread must have stack and queue budget records.
- every DMA-facing buffer must identify memory region and alignment.
- every retained object must identify validity fields and owner.
- map-file evidence must be linked before budgets are treated as proven.
- content/package budgets must be target-profile visible only as abstract limits, not addresses.

---

## Budget Categories

Required categories:

| Category | Purpose |
|---|---|
| owner stacks | ThreadX owner thread stacks |
| owner queues | static queue storage and message pools |
| Platform static buffers | display, audio, storage, communication, sensor, input, shell buffers |
| Engine runtime RAM | runtime host state, state graph interpreter, package context |
| package working RAM | package-visible bounded working memory |
| renderer working RAM | layer planes, compositor scratch, frame state |
| SRAM4 display-DMA/autonomous arena | awake TX scratch overlay, compiled waiting-visual wire payloads, descriptors, queue, metadata, guard |
| retained fast-resume | STOP-class continuity state |
| trace/telemetry | Tracealyzer buffers, SWO/telemetry rings, dashboard capture staging |
| install/storage staging | package import and validation buffers |
| safety margin | reserved headroom per region |

Rules:

- category totals must not hide owner-specific budgets.
- safety margin is a budgeted item, not accidental free space.
- development/instrumented builds may have larger trace budgets than release builds.
- shipping target profiles must not depend on development-only trace buffers.

---

## Retained RAM Contract

For retained structures:
- include `magic`, `version`, and `crc`
- define ownership of writes
- define update frequency limits
- define fallback behavior when invalid

Retained RAM is continuity-only, not durable storage.

## HW6 STOP2 SRAM Retention Policy

HW6 firmware keeps all SRAM banks powered/retained in STOP2 by default. The Platform does not selectively power down SRAM banks, does not copy runtime state into a special retained bank before sleep, and does not reconstruct ordinary RTOS/package/display state after wake.

This is an intentional simplicity and robustness decision. It avoids firmware complexity around pointer validity, stack placement, linker placement, pre-sleep copying, wake reconstruction, and per-bank power bookkeeping. It also avoids the current STM32U575/U585 Stop 2 + LDO erratum class where powering down any SRAM bank can interact badly with some reset sources. Selective SRAM power-down is blocked unless a later measured current budget proves the retained-SRAM current is unacceptable and the erratum/reset behavior is explicitly re-reviewed.

SRAM4 remains special only because it is the Platform display-DMA/autonomous LPBAM arena with validated DMA reachability and ownership rules. It is not special because it is the only retained RAM.
---

## SRAM4 Display-DMA And Autonomous Arena Contract

SRAM4 is reserved for the Platform display-DMA/autonomous arena. It is not general retained runtime memory.

The committed logical/panel framebuffer, RTOS continuity state, package state, renderer working planes, and game-resume state live in ordinary retained SRAM outside the SRAM4 display arena. Because HW6 keeps all SRAM banks retained in STOP2, these objects do not need special pre-sleep copying or reconstruction. SRAM4 contains only the memory that must remain DMA-safe and reachable by the validated display/LPBAM path:

- awake SPI transmit scratch
- autonomous-display wire payload
- LPBAM/LPDMA linked-list descriptors and queue object
- autonomous-slice metadata
- alignment and safety guard

The awake transmit scratch and autonomous payload intentionally share one display-owned arena. They do not need to coexist as independent allocations. The display owner sequences their use:

```text
render/commit in retained runtime RAM
  -> use SRAM4 scratch to present the settled seed frame
  -> wait for awake transfer completion
  -> overlay/reuse SRAM4 scratch as autonomous payload
  -> build and validate descriptors
  -> arm autonomous playback
```

Once an autonomous slice is armed, no normal display present may use the SRAM4 scratch until the display owner has stopped/aborted and unlinked the slice or rebuilt it afterward.

Measured HW5 baseline and provisional HW6 allocation contract:

The final HW6 IOC retains the SRAM4/LPDMA/LPBAM architecture, so these values are the initial HW6 admission model. They are not measured HW6 limits. Descriptor size, linker placement, DMA reachability, and usable guard space must be reconfirmed on HW6 before the profile changes from `pending_validation` to a granted autonomous-display budget. SRAM bank retention itself is not an admission variable: all SRAM banks remain powered in STOP2 unless a future measured optimization explicitly changes the policy.

| Item | Budget |
|---|---:|
| total SRAM4 | 16,384 B |
| safety/alignment reserve | 1,024 B |
| autonomous metadata reservation | 96 B |
| LPDMA queue reservation | 32 B |
| maximum SPI chunk descriptors | 16 x 236 B = 3,776 B |
| retained wire-payload arena, including reusable TX scratch | 11,456 B |
| required awake full-screen TX scratch inside that arena | 3,364 B |

Retained display formulas, validated on HW5 and pending HW6 revalidation:

```text
panel_width = 144
panel_height = 168
line_bytes = panel_width / 8 = 18
framebuffer_bytes = line_bytes * panel_height = 3024
wire_bytes_per_row = 20
chunk_payload_overhead_bytes = 23
max_rows_per_spi_chunk = 48
max_spi_chunks_per_slice = 16
```

With the initial 16-chunk allocation, the HW5-derived conservative autonomous capacity is approximately 553 changed rows across a compiled waiting-visual slice, subject to chunk boundaries, required overlap/guard rows, alignment, and descriptor admission. This is approximately 3.3 full-display equivalents of changed rows, not a promise of a fixed frame count. HW6 package tooling must treat this limit as `pending_validation` until [[HW6_Brought_Up_Tracker]] records the repeated SRAM4/LPBAM measurements.

Rules:

- SRAM4 is not durable storage.
- package artifacts must not encode SRAM4 addresses, linker sections, SPI wire bytes, DMA descriptors, or LPBAM list structure.
- the Platform Display Program Compiler owns target-specific packing and admission.
- normal authoring tools report waiting-visual complexity in PeepOS terms; low-level row/chunk details are advanced diagnostics only.
- exact linker section names and SRAM4 DMA reachability controls must be recorded before Platform freeze; selective SRAM bank power-down is not part of the HW6 baseline policy.
- autonomous-display grants and limits are published through measured target profiles.
- a compiled slice that exceeds the active target budget must use its declared reduced visual or hold fallback; it must not silently keep the CPU awake.

---

## Flash Layout Contract

Define fixed regions for:
- boot/recovery and Platform firmware images
- Platform firmware update staging or metadata if implemented
- package staging area
- package installed area
- save data area
- metadata/index area
- protected persistent fault-log ring near the end of external flash

Layout changes require migration notes and compatibility review.

Rules:

- package installed areas store validated package/content data, not arbitrary native executable game slots.
- Platform firmware update layout is separate from package install layout and follows [[Platform_Firmware_Update_and_Development_Security_Policy]].
- exact bootloader, update-slot, rollback, and production protection layout decisions are deferred until the Platform update/recovery path is intentionally designed.

Persistent fault-log ring rules:

- fixed protected external-flash region
- not host-exposed
- separate from settings, calibration, saves, package blobs, indexes, and staging
- append/ring records with magic, version, sequence, CRC, and compact fault payload
- exact offset and size assigned in the flash-layout pass

---

## Allocation Rules

- No runtime heap allocation in owner threads.
- Any optional allocator must be bounded and documented.
- Compile-time static allocation is the default.
- Package/runtime allocation must be through bounded Engine-owned arenas where used.
- Platform owners must not allocate from package arenas.
- Packages must not choose memory banks, linker sections, or addresses.
- Instrumentation buffers must be build-profile gated.
- Memory-affecting Platform knobs are `compile_time` or `protected_policy` unless a specific owner proves a live-safe boundary.

---

## Target Profile Exposure

Target profiles may publish abstract memory limits:

- package total bytes
- package asset bytes
- package working RAM bytes
- content parameter blob bytes
- save record bytes
- diagnostics/event rate limits
- package scene count and state/action table limits

Target profiles must not publish:

- raw SRAM addresses
- linker symbols
- flash offsets
- stack addresses
- queue addresses
- DMA descriptor addresses
- SRAM4 placement details

---

## Measurement and Enforcement

Required checks:
- map-file stack and section checks in CI
- queue depth stress checks
- retained-RAM corruption tests
- install path flash usage checks
- trace/instrumented build memory overhead reports
- package compatibility budget checks against [[Target_Profile_Schema_Contract]]
- SRAM4 placement and DMA reachability evidence where display DMA or LPBAM is used

Publish results in [[Memory_Reports]].

---

## Validation Cases

1. map file budget report lists every owner stack, queue, and static buffer.
2. SRAM4 report accounts for the TX-scratch/payload overlay, compiled payload bytes, descriptor count, queue/metadata, guard, and alignment; committed framebuffer and retained runtime state are reported in their non-SRAM4 regions.
3. package validation fails if package working RAM or asset limits exceed target profile.
4. instrumented build reports trace/telemetry overhead separately from release build.
5. retained RAM corruption falls back to safe defaults.
6. package artifacts contain no memory addresses or linker-section assumptions.
