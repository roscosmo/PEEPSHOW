# Storage and Installer Contract

This document defines storage ownership, partition model, package install flow, and installer isolation.

For execution-level USB bring-up and regression recovery steps, use `31_usb_msc_bringup_and_recovery.md`.

---

## Ownership (Non-Negotiable)

- `thStorage` is the sole owner of flash and filesystem operations.
- Other threads use `qStorageReq` only.
- No direct filesystem or flash operations outside storage owner.

---

## Region Model

Define explicit regions:
1. settings/config region
2. installed package region
3. transport volume region (host-exposed installer path)

Region metadata must include:
- start address
- size
- erase block size
- ownership rules

---

## Installer Isolation

In installer mode:
- transport volume is host-owned
- local non-installer storage clients are blocked
- rendering/audio/runtime activity is policy-limited per installer contract

Single-writer rule is absolute.

---

## Install Pipeline

Required stages:
1. stage package
2. validate schema and checks
3. commit atomically
4. update installed index
5. report success/failure

Failure path must preserve the last known valid package state.

---

## Installed Index Contract

Installed index must track:
- package ID
- version
- region offsets and sizes
- integrity metadata
- sequence counter

Boot must tolerate partial or corrupt newest record by selecting last valid entry.

---

## Runtime Access Rules

- runtime hosts read package data from installed/raw region or bounded cached RAM
- no FAT reads in active runtime loop
- no mount/unmount churn inside active runtime loop

---

## Save and Settings Rules

- settings writes must be power-fail safe
- save schema versions must support migration paths
- write frequency assumptions and wear strategy must be documented

---

## Validation Cases

1. clean install flow
2. interrupted install rollback
3. return from installer to local mode
4. runtime read behavior without FAT dependency
5. corruption recovery behavior
