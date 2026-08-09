# Storage and Updates

Authoritative specification for external flash usage, FileX (transport volume),
raw MCU-owned regions (settings + installed game blobs), and USB MSC FLASHING
mode in PeepShow V5.

This document defines the storage partition model, ownership rules, blob
install workflow, update rules, and power-fail-safe persistence strategy.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- External flash partition model (raw regions + FAT region)
- Ownership model (thStorage exclusive)
- LevelX + FileX usage (FAT transport volume only)
- Raw settings persistence (power-fail safe, basic wear leveling)
- Game blob install model (install-on-selection)
- FLASHING (USB MSC) mode rules and isolation

Does NOT define:
- Clock control (see power_management.md)
- Thread model and queues (see rtos_architecture.md)
- Asset formats inside blobs (see asset_pipeline.md)
- Audio mixer/runtime (see audio.md)

---

## Design Principles

- Runtime determinism is prioritized over convenience.
- FileX is used for transport only, not runtime streaming.
- Installed game content is read from MCU-owned raw storage, not FAT.
- thStorage is the sole owner of all flash and filesystem operations.
- FLASHING mode isolates USB timing by disabling all other device functions.

---

## Storage Architecture Overview

External flash is partitioned into three conceptual regions:

1. Raw Settings Region (MCU-owned, not FileX, not LevelX)
2. Raw Installed Blobs Region (MCU-owned, not FileX, not LevelX)
3. FAT Transport Volume (LevelX + FileX, optionally exposed via USB MSC)

Summary:

| Region | Access | Wear leveling | Purpose |
|--------|--------|---------------|---------|
| Raw Settings | MCU only | Basic, custom | Calibration, user prefs, device config |
| Raw Installed Blobs | MCU only | Basic, custom | Installed “cartridge” blobs for deterministic runtime reads |
| FAT Transport (FileX) | MCU or Host (MSC) | LevelX | Ingress/inbox for blobs + host file transfer |

---

## Ownership Model (Non-Negotiable)

thStorage is the sole owner of:

- OCTOSPI driver
- Raw flash reads/writes/erase
- LevelX NOR driver
- FileX filesystem mount/unmount and file operations

Other threads must not:
- Touch OCTOSPI HAL
- Call FileX/LevelX
- Perform flash erase/write
- Parse FAT structures

All requests go through qStorageReq.

---

## FAT Transport Volume (LevelX + FileX)

Purpose:
- A host-visible transport volume used to copy game blobs onto the device.
- Not used for live runtime streaming.

Properties:
- Implemented as FileX (FAT) on top of LevelX.
- This volume is the only region exposed via USB MSC.

Expected directory convention (suggested):
- /INBOX/                (new blobs copied here by host)
- /GAMES/<game_id>/       (optional organization)
- /META/                  (optional future)

The specific directory structure is not a contract; the install workflow is.

---

## Raw Regions (MCU-Owned)

Raw regions are not managed by FileX or LevelX.

They use fixed addressing and simple custom persistence mechanisms.

Raw region rules:
- Never mounted or exposed over USB MSC.
- Must remain untouched during FLASHING mode.
- Must be readable deterministically with bounded latency.

---

## Raw Settings Region

Purpose:
- Store user calibration and device preferences:
  - joystick calibration
  - volume
  - brightness
  - accessibility toggles
  - last-selected game/blob
  - any small config needed before FileX is mounted

### Basic Wear Leveling (Explicit)

Settings writes are expected to be frequent relative to blob installs.

A simple log-structured scheme must be used:

- Region contains N fixed-size records.
- Each record includes:
  - sequence counter (monotonic)
  - payload
  - CRC32 over payload (and header fields as required)
- On boot:
  - scan records
  - select highest valid sequence
- On write:
  - write next record
  - when region is full:
    - erase the sector(s)
    - start again at record 0
  - optional: use two-sector ping-pong to improve power-fail safety

Power-fail safety:
- A partially written record must be ignored (CRC fails).
- Previous valid record must remain readable.

---

## Raw Installed Blobs Region

Purpose:
- Store installed “cartridge” blobs in a deterministic runtime store.
- The game engine and audio system read only from installed blobs.

### Install-on-Selection Policy (Authoritative)

Blobs are installed only when:
- the user selects a game/world requiring that blob
- or an explicit “install” operation is invoked (optional future)

No background install is performed implicitly at boot.

This ensures installs occur only at controlled points.

### Basic Wear Policy

Blob installs are expected to be relatively infrequent.

A simple allocation strategy is acceptable:

- Store blobs contiguously with an install index.
- Support delete/uninstall by marking entries inactive.
- Reclaim space via explicit garbage collection later (optional).

If higher churn is expected, use an append-only log with occasional compaction.

### Installed Index (Required)

The raw installed region must include a small index table containing:

- blob_id (and game_id if used)
- version
- offset
- size
- CRC or hash
- install sequence counter or timestamp

Boot behavior:
- index must be validated
- invalid entries must be ignored safely

---

## Blob Model (High-Level)

A blob is a self-contained package representing a discrete world/campaign.

Rules:
- A blob must be self-contained OR depend only on system/public resources.
- Cross-blob asset dependencies are forbidden.
- Blobs may declare links to other blobs by ID for progression.

A blob should contain:
- header (magic, version, blob_id, total_size)
- table of contents (chunks with offsets and sizes)
- chunk payloads (maps, sprites, audio, strings, scripts)
- per-chunk CRC32 (recommended)

Exact chunk schema is defined in asset_pipeline.md.

---

## Runtime Loading Rules

Runtime reads must not depend on FileX.

Runtime reads must be from:
- raw installed blob region
- and system/public resources

The loader may:
- parse manifest into RAM metadata
- cache specific chunks (maps, sprites) into RAM
- stage audio clips into RAM/decoder buffers

The loader must not:
- stream from FAT during gameplay
- block unpredictably on filesystem metadata operations

---

## Update / Replace Workflow

Typical workflow:

1. Enter FLASHING mode.
2. Host mounts FAT transport volume.
3. Host copies blob files into /INBOX/ (or other agreed location).
4. Host safely ejects.
5. Exit FLASHING mode.
6. User selects game/world.
7. thStorage installs blob from FileX → raw installed region.
8. thStorage validates blob CRC and updates installed index.
9. Game loads from raw installed region.

Replacing an installed blob:
- install new version
- validate
- update index to point to new entry
- optionally garbage collect old entry later

---

## Pre-USBX Manifest Bring-up Path (Temporary, Authoritative for this phase)

Until USBX MSC ingest/install is implemented, game package manifest validation is performed using
the fixed raw manifest slot only.

Slot (knob-defined):
- `storage_game_pkg_manifest_addr` (currently `0x00181000`)
- `storage_game_pkg_manifest_max_bytes` (currently `4096`)

Workflow:
1. `thStorage` erases the manifest slot.
2. Host generates manifest bytes (`tools/gen_game_package_manifest.py`) and writes them to slot with ST-LINK/CubeProgrammer/debugger workflow.
3. Firmware validates by calling `APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT`.

Rules:
- This is a bring-up path only.
- No FAT/FileX runtime dependency is introduced for manifest loading in this phase.
- USBX/FileX-based manifest install is deferred to later phase and will supersede this temporary ingest method.

---

## FLASHING Mode (USB MSC)

FLASHING mode exists to isolate USB timing and eliminate concurrency.

In FLASHING mode:
- FileX is unmounted from MCU.
- USB MSC is enabled.
- Host owns the FAT volume.
- Device performs no other functions:
  - no rendering
  - no audio
  - no sensor polling
  - no STOP tick runtime
  - no gameplay

Raw regions remain MCU-owned and are not modified.

Entry and exit sequencing is owned by thPower and coordinated with thStorage.

Single-writer rule is absolute:
- Host writes only while MSC is active.
- MCU writes only while MSC is inactive.

---

## STOP2 Considerations

Before STOP2 entry:
- no active OCTOSPI operation
- transport volume operations completed
- flash placed into deep power-down when appropriate

Install operations must be treated as “awake-only” work:
- do not enter STOP2 mid-install
- installs occur at explicit controlled points

---

## Error Handling Requirements

- Invalid blob files must not brick the device.
- Failed install must not corrupt installed index.
- Index updates must be atomic:
  - write new index entry
  - then commit marker/sequence
- If FileX mount fails:
  - device must remain usable with already installed blobs
  - FLASHING mode remains the recovery path

---

## Invariants (Do Not Violate)

- Only thStorage accesses OCTOSPI, LevelX, FileX, and raw regions.
- FileX is transport only; runtime does not stream from FAT.
- Raw settings use power-fail-safe logging with CRC.
- Installed blobs are validated before becoming active.
- FLASHING mode disables all non-USB functionality.
- No concurrent writers to the FAT volume.

---

## Integration Notes

- LevelX is used only for the FAT transport volume.
- Raw regions implement custom persistence and wear strategy.
- System/public resources must be stable and versioned.
- Installation points must be explicit and user-driven.

---

Last updated: 2026-02-18
