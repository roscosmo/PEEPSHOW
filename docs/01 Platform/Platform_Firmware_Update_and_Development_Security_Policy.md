# Platform Firmware Update And Development Security Policy

This document defines PeepShow Platform policy for development-phase security, future Platform firmware update support, and watchdog timing. HW6 is the active hardware target.

It is intentionally a policy seam, not a final secure-boot design.

Related:

- [[Authority_and_Invariants]]
- [[Architecture_and_Boundaries]]
- [[Storage_and_Installer_Contract]]
- [[USB_Development_Mode_Contract]]
- [[Boot_and_Fault_Supervisor_State_Machine]]
- [[Memory_and_Budgeting_Contract]]
- [[Package_Blob_Format_Contract]]
- [[Package_Contract]]
- [[Brought_Up_Tracker]]

---

## Purpose

PeepShow needs a future path for Platform firmware updates and production hardening without slowing hardware bring-up, OS construction, package tooling, or game-authoring iteration.

Current intent:

- design package, storage, update, and recovery seams so security can fit later
- keep early development and bring-up recoverable
- avoid irreversible protection settings until the Platform lifecycle is stable
- keep package install separate from Platform firmware update
- keep the current validated package-data model as the game runtime model

---

## Current Development Policy

During early PeepOS Platform, Engine, and package-tool development:

- SWD/debug access remains available.
- easy reflashing and recovery are prioritized.
- test and unsigned packages may be accepted by development profiles, while still passing schema, bounds, compatibility, and integrity validation.
- package checksums and chunk CRCs are enforced where the package format requires them.
- signature fields may exist as placeholders, but signature enforcement is not a v1 bring-up requirement.
- irreversible security or option-byte changes must not be enabled during normal bring-up.
- recovery paths must be proven before any protection setting can block direct debug recovery.

Development convenience must not bypass core safety boundaries:

- packages still cannot access HAL, RTOS internals, filesystem paths, flash offsets, DMA descriptors, SRAM placement, or Platform private settings.
- package install still validates package format, schema compatibility, capability compatibility, bounds, and checksums.
- CDC developer workflows remain typed, bounded, owner-routed commands, not raw memory or raw flash access.

---

## Package Install Is Not Platform Firmware Update

PeepOS packages are validated content/runtime data for Engine hosts.

The current package model is:

```text
PeepPkg package data
    -> package manager validation
    -> Engine runtime hosts
    -> Platform APIs
```

Package install must not be silently reinterpreted as native executable game-image installation.

Package install may install:

- manifests
- runtime unit tables
- state graphs and action tables
- scene/module data
- assets
- audio data
- sensor/communication/time/power profiles
- save schemas
- bounded diagnostics metadata

Package install must not:

- replace Platform firmware
- rewrite a bootloader or recovery region
- install arbitrary native executable code into an internal app slot
- bypass Engine/package validation
- change Platform knobs, Platform settings, storage policy, power policy, or hardware policy

If native executable package support is ever proposed, it must be a separate future architecture decision with a dedicated ABI, memory, update, validation, recovery, and target-profile contract.

---

## Platform Firmware Update Boundary

Platform firmware update is a system-owned operation.

It is separate from:

- package install
- package asset/blob install
- package save/settings migration
- normal MSC staging/export
- CDC package upload

Future Platform firmware updates may share a staging transport with packages, but they must enter a distinct Platform update/recovery flow before any firmware image is applied.

Required boundary rules:

- only Platform-owned update code may validate and apply Platform firmware updates.
- packages, Engine runtime hosts, package tools, and Reference Game code cannot request raw Platform firmware writes.
- update artifacts must be versioned and integrity-checked before apply.
- failed update handling must preserve a documented recovery path.
- exact bootloader layout, update slot model, secure verification chain, and rollback model are deferred until flash layout and Platform lifecycle are stable.

---

## Future Update Transfer Paths

Normal-user update transfer should prefer the universal USB path:

```text
USB data-host gate
    -> MSC staging/export
    -> user copies Platform update artifact
    -> firmware reclaims staging
    -> Platform update flow validates and applies
```

Developer update transfer may use CDC:

```text
CDC developer mode
    -> structured update upload commands
    -> firmware-owned staging
    -> Platform update flow validates and applies
```

Both paths should feed the same staged update artifact format and Platform-owned update apply logic.

Rules:

- MSC is a staging/transfer mechanism, not a trust boundary.
- CDC is a developer control path, not an arbitrary flash programmer.
- the firmware update flow must not rely on simultaneous MSC and CDC in v1.
- ROM bootloader or DFU-style recovery paths may be validated as development or recovery tools, but they are not the default package install model.
- bring-up may continue to use SWD/debugger workflows until the update flow is intentionally implemented.

---

## Security Enforcement Phasing

Security should be designed in phases.

### Phase 1: Architecture And Bring-up

Define and preserve:

- storage region ownership
- package/install state machines
- package version and integrity metadata
- placeholder authentication/signature fields
- recovery and safe-mode routing
- reset-cause capture
- development recovery workflows

Do not enforce production lock-down in this phase.

### Phase 2: Tooling And Runtime Stability

Stabilize:

- package build/export
- MSC staging and CDC developer upload
- installer validation and commit
- Engine runtime lifecycle
- digital twin parity
- fault logging and recovery behavior
- platform update artifact shape if needed

Security checks may remain development-permissive while compatibility, recovery, and update flows are proven.

### Phase 3: Production Hardening

Only after Platform update/recovery behavior is stable, decide:

- package signature enforcement
- Platform update signature enforcement
- anti-rollback policy
- production option-byte settings
- debug lock policy
- boot/platform write-protection policy
- dev-unit and consumer-unit divergence
- whether TrustZone, MPU hardening, RDP, PCROP, write protection, or similar MCU features are needed

No irreversible security setting should be enabled until the recovery path is proven and documented.

---

## Developer And Consumer Edition Intent

Developer profiles may allow:

- SWD/debug access
- unsigned or dev-signed packages where profile policy allows
- CDC developer commands
- live-safe Platform tuning
- telemetry and trace workflows
- rapid reflashing and recovery

Developer profiles still must preserve:

- package validation
- Platform/package boundary
- protected storage ownership
- no raw memory or raw flash commands through normal developer protocols
- recovery availability

Consumer profiles may later require:

- debug lock-down
- signed packages
- signed Platform updates
- protected boot/platform regions
- limited diagnostics
- official recovery/update flow only

The exact consumer protection settings are deferred until production hardening.

---

## Watchdog Reliability Policy

The watchdog is a release-hardening reliability feature, not an early bring-up requirement.

Rationale:

- the device has a physical reset path for user recovery during development
- early watchdog enablement can obscure bring-up failures and complicate debugging
- watchdog behavior must be proven against low-power residency, storage commits, package saves, and update/recovery flows

Long-term value:

- a long-running virtual-pet device may hang when unattended
- a watchdog reset can return the device to a known boot path
- reset-cause logging can identify that the reset was watchdog-driven
- package/runtime recovery can resume from the last valid save or fast-resume state where available

Deferred watchdog requirements before enabling in release builds:

- reset-cause capture is reliable
- persistent fault logging is available or an acceptable early-boot diagnostic fallback exists
- package save and Platform settings writes preserve previous valid records on failure
- owner-thread health reporting semantics are defined
- watchdog refresh policy is owned by a supervisor, not scattered across owner threads
- sleep/resume behavior with watchdog enabled is validated
- watchdog reset recovery does not corrupt installer, update, storage, save, or package metadata

Until those conditions are met, watchdog behavior remains `deferred_release_hardening`.

---

## Validation Requirements

Before Platform firmware update is considered supported:

1. staged update artifact is distinguishable from `PeepPkg` packages.
2. package install rejects Platform update artifacts.
3. Platform update flow rejects malformed, incompatible, or corrupted update artifacts.
4. update apply path has a documented failure and recovery route.
5. MSC staging transfer and CDC developer upload feed the same Platform-owned update validation path where both are supported.
6. recovery behavior is proven before any protection setting blocks direct debug recovery.

Before production security enforcement:

1. package install/update behavior is stable.
2. Platform update/recovery behavior is stable.
3. dev and consumer profiles are explicitly defined.
4. key/signature material handling is documented if signatures are enforced.
5. option-byte, RDP, PCROP, write-protection, TrustZone, MPU, or related hardware protection choices are reviewed against the recovery plan.

Before watchdog release enablement:

1. reset cause is captured and exposed through boot/fault diagnostics.
2. watchdog reset is distinguishable from user reset, power reset, and fault recovery where hardware allows.
3. owner health reporting and refresh policy are deterministic.
4. sleep/resume tests pass with watchdog policy active.
5. interrupted save, settings, installer, and update scenarios preserve the last valid state.

---

## Rule

Design the seams now.

Keep bring-up unlocked and recoverable.

Do not turn package install into native firmware update.

Do not enable irreversible security or watchdog enforcement before recovery, update, storage, and sleep behavior are proven.
