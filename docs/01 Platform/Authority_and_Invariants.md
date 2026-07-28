# Authority and Cross-Cutting Invariants

This document is the single source of truth for rules that apply across PeepShow subsystems and hardware targets.

If another document conflicts with this one, resolve the conflict immediately.

---

## Scope

Defines:

- Platform, Engine, and Reference Game authority
- ownership and concurrency invariants
- timing and cadence invariants
- storage and installer invariants
- firmware update and development security invariants
- determinism and debug invariants
- documentation priority

Does not define:

- board electrical details, see the active target contract in [[Hardware_Index]]; HW6 authority begins at [[HW6_Hardware_Revision_Contract]]
- per-subsystem FSM details, see [[Subsystem_State_Machines]]
- Reference Game mechanics, see [[Reference_Game_Index]]

---

## Layer Authority

Platform owns:

- hardware behavior
- RTOS ownership
- clocks and sleep policy
- peripheral access
- storage and USB arbitration
- fault handling
- bring-up evidence

Engine owns:

- reusable runtime abstractions
- package/content contracts
- scene/input/rendering abstractions above Platform APIs
- game-development SDK rules
- authoring-source concepts such as templates, Authoring Kits, prefabs, behavior graphs, behavior macros, and their compiler contracts

Reference Game owns:

- content
- mechanics
- creatures
- encounters
- lore
- balancing
- game-specific state machines

The Reference Game may request capabilities. It may not redefine Platform behavior.

A generic subsystem or API contract does not grant physical target support. Target profiles publish only capabilities supported by the selected hardware contract and target-qualified evidence.

---

## Canonical Runtime Classes

Use these exact runtime class tokens until explicitly replaced by a newer Engine contract:

- `SHELL`
- `LP_GRAPH`
- `LP_MODULE`
- `RT_SCENE`
- `INSTALLER`

These are runtime class tokens, not authoring reuse names.

Use `Authoring Kit` for reusable gameplay systems such as dialogue, shop, NPC, evolution, inventory, pathing, or quest logic. Do not use `module` as the general authoring name for gameplay reuse. `LP_MODULE` remains a runtime class token, and hardware modules remain Platform/Hardware terminology.

---

## Ownership Model

- Every peripheral and shared subsystem has exactly one Platform owner.
- Other layers must send typed requests only.
- No cross-thread direct HAL/LL register access.
- ISR code must signal and return immediately.

Request payload rules:

- fixed-size POD structs
- no transient pointer ownership
- no function pointers
- bounded work only

---

## Timing Model

- Reactive scheduled cadence must be RTC/event driven.
- Real-time cadence must be frame-scheduled and deterministic.
- Reactive runtime work executes as bounded event transactions. Once the current transaction and its Engine actions settle, the runtime yields and the Platform selects the deepest compatible sleep state.
- Waiting for fresh input, a schedule, or another admitted event is not an awake runtime mode.
- Display motion while a reactive runtime is waiting is presentation behavior, not evidence that package logic is running.
- No mode transition is complete until HAL and RTOS timebases are valid.

Time-domain labels are mandatory for Platform timing knobs:

- `threadx`
- `hal_ms`
- `knob_rtos_tick_hz`

---

## Power and Clock Invariants

- Platform power owner is sole owner of sleep class and clock transitions.
- Quiesce-before-sleep and resume-before-validate are mandatory.
- No clock changes during active DMA or active bus transaction.
- No STOP entry while critical owners are unquiesced.
- Engine and Reference Game express power intent only.
- `REACTIVE` and `REALTIME` are Engine execution semantics. `STOP2`, LPBAM, LPDMA, and autonomous-display setup are Platform implementation details.
- Execution semantics select Platform operating-point policy objectives, not literal clock frequencies.
- Reactive active bursts optimize measured energy per completed event-to-yield transaction while meeting response-latency limits.
- Realtime execution uses the lowest measured operating point that satisfies frame, audio, sensor, and owner deadlines with required margin.
- Shipping clock/voltage operating points and any switching hysteresis require measurement evidence from the active shipping target. HW6 values require HW6 evidence; HW5 measurements are regression references only.
- `HOLD` and animated waiting visuals are presentation choices attached to a reactive state; they are not package-controlled hardware modes.

---

## Input Lock Invariants

- Automatic input locking is a package policy implemented and enforced by PeepOS. A package may enable or disable automatic locking.
- The active target/system policy owns the input-lock timeout. A package cannot author a replacement timeout.
- When automatic locking is enabled and its input-lock timer expires, only `START` may wake and unlock normal package interaction.
- The physical `START` press used to unlock is consumed by PeepOS and is not delivered as a package action. The Engine receives a symbolic unlock lifecycle event instead.
- A package may preserve its current state, transition to a declared package state, or exit to the PeepOS shell when locking occurs.
- A package may defer locking only for statically bounded work. Unbounded lock deferral is forbidden.
- Package-authored gameplay inactivity timers are normal bounded schedules and state transitions. They are separate from the PeepOS input-lock policy.

---

## Storage and Installer Invariants

- Storage owner is sole owner of flash and filesystem operations.
- Engine and Reference Game must not use FAT for active runtime execution.
- `INSTALLER` is single-writer mode for host-visible transport.
- Non-installer subsystems are isolated while installer path is active.
- Package install is not Platform firmware update.
- Platform firmware update is a Platform-owned recovery/update flow, not an Engine/package runtime operation.

---

## Firmware Update and Development Security Invariants

- Development and bring-up builds must remain recoverable.
- Do not enable irreversible debug lock-down, option-byte protection, RDP, PCROP, write protection, TrustZone, MPU hardening, or equivalent production security enforcement until the recovery/update path is proven and documented.
- Security metadata seams such as versions, hashes, checksums, and future signature fields may exist before enforcement.
- Package validation remains mandatory even when signature enforcement is deferred.
- Native executable package installation into an internal app slot is not the current package model. Any future native executable package path requires a separate architecture contract.
- Watchdog enforcement is deferred release hardening until reset-cause logging, owner health reporting, sleep/resume, and interrupted storage/update recovery are proven.

---

## Determinism Invariants

- No unbounded loops in runtime-critical paths.
- No hidden retries or random backoff logic.
- No runtime dynamic allocation unless explicitly documented and approved.
- No filesystem streaming in active runtime loops.

---

## Debug Invariants

- HardFault capture is mandatory.
- Breakpoint use must be strategic and bounded.
- Structured trace events are preferred over heavy halt-based debugging.
- STOP behavior must be validated with evidence, not inference.

---

## Knobs Invariants

- All tunable Platform firmware constants flow through the Platform knobs pipeline.
- Generated outputs are never edited manually.
- Knob changes require regeneration and rebuild.
- Knobs must not silently alter architecture boundaries.
- Platform knobs are not part of the game-authoring API.
- Package-authored balancing values are content parameters, not Platform knobs.

---

## Game Documentation Invariant

Game documentation may advance during hardware bring-up.

Game implementation may not drive Platform architecture.

Game notes may define intent and desired capabilities. They may not define:

- pin choices
- peripheral ownership
- DMA policy
- clock policy
- sleep policy
- storage ownership
- hardware fault handling

See [[Game_Documentation_Boundary]].

---

## Document Priority

When implementing:

1. this document
2. relevant Hardware contract
3. relevant Platform subsystem contract
4. relevant Engine API contract
5. bring-up evidence and runbooks
6. Game or Reference Game intent notes

Any rule conflict must be fixed in docs immediately.
