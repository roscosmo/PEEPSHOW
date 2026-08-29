# Storage and Installer Contract

This document defines active HW6 storage ownership, flash behavior, region model, USB staging/export mode, and installer isolation. Retained HW5 behavior must be revalidated on HW6.

For execution-level USB bring-up and regression recovery steps, use [[USB_MSC_Bring-up_and_Recovery_Runbook]].

Developer USB mode and the MSC/CDC personality split are defined in [[USB_Development_Mode_Contract]].

Platform firmware update and development security phasing are defined in [[Platform_Firmware_Update_and_Development_Security_Policy]].

## Hardware and Middleware

| Item | HW6 Selection / Intent |
|---|---|
| External flash | `AT25SL128A` serial NOR flash |
| Bus | `OCTOSPI1` quad mode |
| DMA RX | `GPDMA1_CH4`, `GPDMA1_REQUEST_OCTOSPI1` |
| DMA TX | `GPDMA1_CH5`, `GPDMA1_REQUEST_OCTOSPI1` |
| Filesystem middleware | FileX |
| Flash translation | LevelX custom NOR interface |
| USB export | USBX MSC |
| USB developer control | USBX CDC, dev-only, mutually exclusive with MSC in v1 |

External flash is the only persistent storage besides internal MCU flash.

## Ownership

- `thStorage` is the sole owner of external flash, OCTOSPI1, storage DMA, FileX, LevelX, USB MSC media, mount state, and host-export state.
- Other threads use `qStorageReq` only.
- No direct filesystem or flash operations are allowed outside `thStorage`.
- Engine and Reference Game code consume package, asset, save, and settings APIs only.

## Region Model

External `AT25SL128A` regions use the following fixed HW6 map. All boundaries
are aligned to the `4096`-byte erase sector.

| Region | Start | Length | Notes |
|---|---:|---:|---|
| settings/config | `0x00000000` | `64 KiB` | protected |
| communication bonding | `0x00010000` | `64 KiB` | protected |
| calibration | `0x00020000` | `64 KiB` | protected |
| save data | `0x00030000` | `512 KiB` | protected |
| installed index/metadata | `0x000B0000` | `64 KiB` | protected |
| installed package/blob | `0x000C0000` | `10 MiB` | protected; two `5 MiB` slots |
| USB staging/export | `0x00AC0000` | `5 MiB` | host exposed only during MSC |
| persistent fault log ring | `0x00FC0000` | `192 KiB` | protected |
| reserved tail | `0x00FF0000` | `60 KiB` | protected |
| bring-up scratch | `0x00FFF000` | `4 KiB` | destructive tests only |

Required regions:

| Region | Purpose | Host Exposed | Owner |
|---|---|---|---|
| settings/config | Platform settings | No | `thStorage` |
| communication bonding | BLE pairing/bonding data | No | `thStorage` through `thComm` request |
| calibration | joystick/input/display/sensor calibration | No | `thStorage` |
| save data | Reference Game and package saves | No | `thStorage` through save API |
| installed package/blob | installed game/content raw blob storage | No | `thStorage` / Engine package API |
| installed index/metadata | active package index, versions, integrity data | No | `thStorage` / package manager |
| Platform update staging metadata | staged Platform update artifact state, if implemented | No | Platform update flow / `thStorage` |
| persistent fault log ring | boot/fault records, reset evidence, crash summaries | No | `thStorage` through fault supervisor request |
| USB staging/export | package staging, host transfer, debug export surface | Yes | host while exported, `thStorage` otherwise |
| logs/screenshots/debug export | copied/exported diagnostic artifacts | Indirect only | `thStorage` |

Rules:

- settings, calibration, communication bonding records, saves, installed blobs, installed indexes, and persistent fault logs are never directly host-writable.
- USB MSC exposes only staging/export storage.
- VBUS presence alone is external-power evidence, not evidence that a USB data host is available for MSC.
- v1 USB MSC and USB CDC developer control are mutually exclusive personalities.
- persistent fault logs remain in a protected ring region; firmware may copy/export diagnostic summaries into the staging/export volume.
- host access must never expose internal storage regions directly.
- Platform firmware update artifacts may be transferred through staging/export or CDC developer upload in the future, but applying them is a distinct Platform-owned update/recovery flow.
- package install must not rewrite Platform firmware, bootloader/recovery regions, or native executable app slots.

### Installed Package Slots And Index

The first persistent installer supports one active package with an A/B update
pair. This is a physical durability mechanism, not a user-visible two-package
catalog.

- package slot A is `0x000C0000..0x005BFFFF` (`5 MiB`);
- package slot B is `0x005C0000..0x00ABFFFF` (`5 MiB`);
- one `.egg` must fit entirely in one slot;
- package bytes begin at the first byte of the selected slot and remain in the
  canonical `.egg` format without a storage-specific wrapper;
- unused bytes after `package_size_bytes` are ignored;
- the currently indexed slot is never erased or programmed during replacement.

The package-index region begins with two independent erase-sector records:

- index A is `0x000B0000..0x000B0FFF`;
- index B is `0x000B1000..0x000B1FFF`;
- `0x000B2000..0x000BFFFF` remains reserved for future catalog expansion;
- each record contains format version, generation, active slot, package size,
  package identity hash, package SHA-256, flags, and a record CRC32;
- each record has an exact commit marker stored separately from its CRC-covered
  body and programmed only after the body has been read back successfully;
- a record is valid only when its marker, format, bounds, slot mapping, and
  CRC32 all validate;
- boot selects the valid record with the newer generation; one valid record is
  sufficient, and no valid record means no installed package;
- generation comparison must remain deterministic across `uint32_t` wrap.

Index v1 uses little-endian fields. Its CRC-covered body is exactly `256`
bytes: magic `EGI1`, format version `1`, body size, generation, active slot,
package size, 64-bit package identity hash, zero flags, the package SHA-256,
CRC32, then erased `0xFF` reserved bytes. The CRC field is treated as zero
while calculating CRC32. The exact commit marker `CMIT` is stored at sector
offset `0x100`, outside the body, and the remainder of the `4 KiB` sector stays
erased. Unknown nonzero flags or programmed reserved bytes invalidate a v1
record.

Atomic replacement order is mandatory:

1. select the slot not named by the current valid index;
2. erase only that inactive slot's required sectors;
3. copy the staged `.egg` into the inactive slot through `thStorage`;
4. read back and validate the installed bytes, container integrity, and SHA-256;
5. erase the inactive index sector;
6. write and read-verify the new index body;
7. program and verify the commit marker last;
8. treat the new generation as active without erasing the previous package or
   previous index record.

Any interruption before step 7 leaves the previous index and package active.
An interruption during the commit-marker program leaves the new record invalid
unless the exact marker and complete CRC-covered body both validate. Cleanup of
the old slot or old index is deferred until a later replacement needs that
inactive location.

The HW6 FW0 USB vertical slice now exercises this physical replacement path
end to end. MSC reclaim is transport-only: it closes the host-exported
filesystem path and parks ownership without automatically installing,
launching, or publishing package prompts. The PACKAGE menu separates `USB
FLASH` from `PACKAGE INSTALL`; only the package-install path explicitly asks
`thStorage` to scan the reclaimed staging volume. A valid single `.egg` then
enables an explicit install action that copies the package into
bounded RAM, writes and verifies the inactive package slot, commits the inactive
index record last, parks flash, and publishes the selected generation as
installed runtime RAM. The package browser changes from `INSTALLING` to
`INSTALLED` after the persistent install succeeds. A separate explicit launch
action activates the installed STATE scene; failure returns the browser to an
explicit error state so STOP2 is not permanently vetoed.

The current bring-up admission check before persistent commit is still narrower
than the production rule in step 4: it checks the bounded `PKG1` envelope and
footer, then byte-verifies the programmed package. SHA-256, complete container,
chunk, and scene-schema validation currently runs during the immediate runtime
activation. Production completion must move the required integrity checks
before the commit marker so a semantically invalid package can never become the
selected generation.

The current `65536`-byte staged-RAM source remains a bring-up activation cache,
not the installed slot size. Persistent layout and index metadata must support
the full `5 MiB` slot. Runtime metadata and small prepared assets may be cached
in normal SRAM; large assets use bounded storage-owner reads through the package
asset API and are never read through FileX.

## Persistent Fault Log Region

Reserve a small protected ring region near the end of external flash for persistent fault evidence.

Rules:

- region is not host-exposed and is not part of USB staging/export
- region is separate from settings, calibration, bonding, saves, installed packages, indexes, and staging
- writes are append/ring style with magic, version, sequence number, timestamp or boot counter, CRC, and fault class
- a failed write must preserve the previous valid record
- early boot faults before storage is mounted use debugger/SWO/serial where available, then later faults use the protected ring
- export to host is by firmware-copy into staging/export only, never direct mount exposure

Exact offset and size are assigned during the flash-layout pass.

## Data Source Rules

- Runtime/package assets are read through [[Package_Asset_Loading_API_Contract]] from installed raw blob storage or bounded cached RAM.
- Installed package blobs use the `PeepPkg` container defined in [[Package_Blob_Format_Contract]].
- FAT/FileX is staging/debug export only.
- No FileX/FAT reads are allowed during active gameplay/audio runtime loops.
- No mount/unmount churn is allowed inside active runtime loops.
- Music streaming sources must be raw installed blob storage into bounded buffers, not FAT.

## External Flash Device FSM

| State | Meaning |
|---|---|
| `FLASH_OFF` | flash path inactive or not yet touched |
| `FLASH_RESET` | bus/device reset sequence in progress |
| `FLASH_PROBE` | JEDEC/device identity and liveness probe |
| `FLASH_CONFIG` | quad/OCTOSPI configuration and protection checks |
| `FLASH_READY` | flash ready for read/program/erase requests |
| `FLASH_BUSY_READ` | read transfer active |
| `FLASH_BUSY_PROGRAM` | program transfer active |
| `FLASH_BUSY_ERASE` | erase operation active |
| `FLASH_DEEP_POWER_DOWN` | flash placed in low-power deep power-down |
| `FLASH_RECOVERING` | bounded recovery after bus/device error |
| `FLASH_ERROR` | flash unavailable or recovery exhausted |

Flash rules:

- deep power-down whenever idle and Platform policy allows
- no sleep entry during active read/program/erase
- every operation has an explicit timeout
- wake/resume must revalidate device liveness before use
- recovery retries are bounded

## Storage Ownership FSM

| State | Meaning |
|---|---|
| `STORAGE_OFFLINE` | storage unavailable, not initialized, or failed |
| `STORAGE_INIT` | storage owner initializing objects and flash path |
| `STORAGE_FLASH_READY` | flash device ready, filesystem not yet mounted |
| `STORAGE_LOCAL_MOUNT` | local FileX/LevelX mount in progress |
| `STORAGE_LOCAL_READY` | firmware owns storage and internal regions are available |
| `STORAGE_QUIESCE_LOCAL` | local users are being drained before export/install/sleep |
| `STORAGE_PREPARE_USB` | staging/export volume is prepared for host ownership |
| `STORAGE_USB_STAGING_EXPORTED` | host owns USB staging/export volume |
| `STORAGE_USB_STAGING_DIRTY` | host wrote or changed staging/export volume |
| `STORAGE_USB_RELEASE` | host export is ending and firmware is reclaiming ownership |
| `STORAGE_INSTALLING` | package install/commit operation active |
| `STORAGE_RECOVERING` | bounded recovery path active |
| `STORAGE_SAFE_MODE` | normal shell blocked because storage/settings/calibration unavailable |
| `STORAGE_ERROR` | unrecovered storage fault |

Storage rules:

- normal shell requires storage because settings and joystick calibration are required for usable operation
- storage failure routes to safe mode, not normal shell
- all local clients must be blocked before USB export
- firmware must reclaim and rescan staging/export after USB release
- install commit must preserve last known valid package/index state

## USB Staging / Export FSM

| State | Meaning |
|---|---|
| `USB_STAGE_OFF` | USB staging/export inactive |
| `USB_STAGE_PREPARE` | staging volume prepared and local users blocked |
| `USB_STAGE_EXPORTED` | USB MSC visible to host |
| `USB_STAGE_HOST_ACTIVE` | host IO observed |
| `USB_STAGE_HOST_DIRTY` | host changed staging/export volume |
| `USB_STAGE_RELEASE_REQUESTED` | user/system requested exit from flashing/export mode |
| `USB_STAGE_RESCAN` | firmware reclaimed volume and is checking contents |
| `USB_STAGE_READY_FOR_INSTALL` | staged package/debug data ready for processing |
| `USB_STAGE_ERROR` | USB staging/export fault |

USB export rules:

- host owns the staging/export FAT volume while MSC is active
- MCU FileX/FAT remains unmounted while host owns the staging/export volume
- USB CDC developer control is not active while MSC owns the staging/export volume in v1
- `B` button is the minimal local exit/cancel input during flashing/export mode
- display may show a static "flashing" or installer screen and then remain mostly inactive
- runtime rendering/audio/gameplay are disabled or policy-limited during export
- MSC export may run while VBUS is present; VBUS is expected during active host export but is not sufficient to enter export by itself
- `thStorage` owns FileX/LevelX, the MSC media bridge, and export/reclaim sequencing, but USB and OCTOSPI clock selection is requested through `thPower`; FW0 routes storage service requests through the storage clock requester slot so MSC export/reclaim request `USB_DEVICE_ACTIVE | OCTOSPI_ACTIVE`, explicit flash/staging provisioning and staged package loads request `OCTOSPI_ACTIVE`, and cleanup releases the storage requester only after FileX/LevelX are closed and the physical flash operation is complete. Flash wake/revalidation and flash deep-power-down commands must both execute while `OCTOSPI_ACTIVE` is held. STOP2 resume and quiesce may therefore acquire this capability temporarily around the corresponding storage-owner command, then release it after the flash reaches its required physical state.
- HW6 evidence `EV-HW6-20260812-P1-CLOCKSTORAGE-039` validates the MSC export/reclaim portion of this requester wrapper: active MSC held the storage requester at `ST=0x3`, selected `CLK_IO_HIGH`, and blocked STOP2; reclaim released storage to `ST=0x0`, returned to `REACTIVE_BASE`, cleared required/managed/readback clock domains, and parked USB/VDDUSB/HSI48 plus PLL2.
- HW6 evidence `EV-HW6-20260812-P1-FLASHINIT-040` validates explicit flash/staging provisioning through the same requester wrapper: flash init requested `OCTOSPI_ACTIVE`, completed erase/LevelX/FileX/FAT provisioning with status `0x0`, released storage clocks with flash/release statuses `0x0/0x0`, and the next MSC export mounted as an intentionally empty staging volume.
- HW6 evidence `EV-HW6-20260814-P1-STORAGEATTACH-063` validates the reset-safe non-destructive attach/check path: attach requested `OCTOSPI_ACTIVE`, woke/probed flash, read JEDEC `1f 42 18`, validated layout, returned storage to `STORAGE_FLASH_READY` with flash in `FLASH_DEEP_POWER_DOWN`, released the storage clock requester, and parked OCTOSPI without erase or format.
- HW6 evidence `EV-HW6-20260814-P1-STOP2STORAGE-064` validates STOP2 post-wake storage recovery: when storage was already attached and flash was in deep power-down, `thPower` temporarily applied storage `OCTOSPI_ACTIVE` during post-STOP owner resume before asking storage/flash to revalidate, then released the requester and re-parked OCTOSPI.
- export prep must not use detached USB-park checks that reject VBUS; detached parking and active MSC export are different states
- reclaim disables the bridge export policy, stops/disconnects USB, closes FileX/LevelX, returns the clock policy to base, then records whether staging/export needs firmware rescan
- normal MSC export must only open an already-provisioned staging/export volume; it must not erase, format, or auto-repair flash as a side effect of host export
- destructive staging/export provisioning is a separate `thStorage` command for manufacturing, bring-up, or explicit recovery, and must be initiated intentionally before MSC export is retried
- explicit provisioning may bootstrap storage/flash from `STORAGE_OFFLINE` / `FLASH_OFF` to the normal flash-ready precondition before it erases and formats only the USB staging/export region
- explicit provisioning may emit sparse SWO lifecycle tokens (`REQ`, `WAK`, `LAY`, `ERS`, `FMT`, `DON`, `ERR`) under [[Debug_and_Observability]] so operators do not need to pause the target to guess completion timing
- MSC entry/exit is an OS service request routed to `thStorage`; temporary GDB helpers are callers of that route, not a separate storage lifecycle
- UI-originated MSC entry and package-install overlay actions must pass system-action admission before `thStorage` receives the request. Admission does not own flash or USB; it only decides whether the action may start now, whether active package runtime must be suspended first, or whether the request is denied because another system overlay is already active. MSC exit remains admitted while the overlay is active.
- HW6 evidence `EV-HW6-20260814-P1-ADMISSION-053` validates this admission layer with the dry-run MSC-enter helper: active `LP_MODULE` runtime was suspended by `thRuntime` before admission returned success, without starting USB MSC.
- FW0 package-page USB transfer scaffold is another caller of that same route: `thUI` may request MSC enter/exit, but `thStorage` still owns export, host ownership, FileX/LevelX open/close, reclaim, and any diagnostic rescan. MSC is transport only: reclaim must not automatically install, launch, or publish package prompts just because files were copied.
- during HW6 FW0 USB/storage bring-up, explicit provisioning and MSC service requests may also show temporary display cues through `thDisplay`; these cues are observation-only and must not decide storage, USB, erase, format, or reclaim behavior
- MSC UI is an overlay: successful reclaim restores the UI page/state underneath the MSC cue; error and recovery states may remain on the MSC overlay until handled.
- FW0 reclaim may run a bounded staging/export root-directory classifier for diagnostics: it opens FileX/LevelX only under `thStorage`, classifies the reclaimed volume as `EMPTY`, `UNSUPPORTED`, `PACKAGE_CANDIDATE`, `MULTIPLE`, or `ERROR`, closes FileX/LevelX before returning, and performs no package import or install writes. This classifier must not decide whether MSC reclaim itself succeeded; safe USB teardown/parking is the transport completion condition. Directories are counted for diagnostics but do not make a single package file unsupported, because host operating systems may create metadata directories. `PACKAGE_CANDIDATE` is a filename-level hint only for exactly one `.egg` file, not an automatic install prompt.
- FW0 runs a read-only minimum package-envelope validator for one clean package candidate. This validator reopens the staging/export volume under `thStorage`, opens the package file for read, reads at most the first 64 bytes, requires the first four bytes to be `PKG1`, records the first 16 bytes and FileX/LevelX statuses for GDB evidence, and closes FileX/LevelX before returning. This is prompt admission only; it is not complete package validation.
- Selecting `PACKAGE INSTALL` from the PACKAGE menu asks `thStorage` to scan the reclaimed staging volume. This is the first point where product UI decides whether copied files should be considered for package install.
- Pressing `A` on a valid package prompt asks `thStorage` to reopen the single staged `.egg`, reject files outside the current fixed `65536`-byte runtime-cache capacity, read the complete file, verify the bounded envelope and declared size, and close FileX/LevelX. The accepted image is then written and byte-verified in the inactive A/B raw package slot, followed by the inactive index body and commit marker last.
- After commit, `thStorage` publishes the selected generation through `INSTALLED_RAM` and reports `INSTALLED` to the package browser. Pressing `A` on the installed prompt then asks `thRuntime` to perform SHA-256, header CRC, chunk CRC, graph/render/waiting-schema validation, and STATE activation. Install and launch are separate user actions.
- Runtime never reads FAT/FileX. A package must be exited before installer admission replaces the shared runtime cache. The persistent A/B write/index/select/launch path is implemented and target-proven; full validation before commit, reset injection at every install stage, automatic boot activation, uninstall, last-known-good fallback policy, and package quarantine remain open.
- HW6 evidence `EV-HW6-20260813-P1-PKGMSC-043` validates the earlier package-page force-rescan path that auto-prompted after reclaim. That behavior is now classified as bring-up scaffolding only; product MSC remains transport-only, and package selection/install belongs to the package browser. HW6 evidence `EV-HW6-20260813-P1-RUNTIME-044` validates the runtime-host installer overlay around that earlier scaffold; it does not define final package-browser UX.
- if MSC export detects an unformatted or invalid LevelX/FileX staging volume, it reports recovery-required state and leaves formatting to the explicit provisioning command
- normal boot may ask `thStorage` to run a USB boot-park cleanup command; this command only parks generated USB device hardware and refreshes clock readback, and must not mount FileX/LevelX, initialize package storage, expose MSC, or prove storage readiness

## USB Data-Host Detection And MSC Gate

VBUS detection and MSC export are separate lifecycle gates.

VBUS may:

- wake power/USB policy
- enable charger and external-power handling
- allow lightweight USB protocol detection where power policy permits it

VBUS must not:

- directly prompt for installer/export mode
- directly hand staging/export storage to USB MSC
- directly quiesce active gameplay or runtime storage ownership

Preferred v1 gate model:

| Gate | Meaning | Storage Consequence |
|---|---|---|
| `VBUS_PRESENT` | external power is present | charging/power policy only |
| `USB_ACTIVITY_DETECTED` | USB reset or equivalent protocol activity from a real data host was observed | host detection may continue; no MSC storage export |
| `USB_ENUMERATED` | a lightweight USB personality reached successful host enumeration/configuration | PeepOS may expose a USB-host-available status or offer installer/export |
| `MSC_AVAILABLE` | user/system policy allows the installer/export offer | UI may prompt for MSC entry |
| `MSC_ACTIVE` | installer/export entry accepted and MSC personality active | `thStorage` quiesces local users and exports staging storage |

Rules:

- actual USB protocol activity or successful enumeration is required before MSC availability is offered.
- Diagnostics must label one-time MCU VBUS samples as external-power samples, not host proof; MSC activation, USB configuration, and SCSI/media callbacks are data-host proof in FW0 export captures.
- FW0 exposes a USB availability scaffold in the owner state-machine probe: external-power-present is PMIC/MCU VBUS evidence, data-host-seen requires MSC SCSI/media traffic, msc-available is future UI eligibility only, and msc-active means host currently owns staging/export. This state does not start MSC by itself.
- a charger or USB-C power bank that provides VBUS without usable USB data must remain an external-power/charging case and must not trigger an MSC prompt.
- lightweight host detection/enumeration may run while gameplay continues when Platform policy allows it.
- pre-MSC detection should be lightweight control/status or protocol detection only; it is neither MSC storage export nor the CDC developer personality unless explicit dev-mode policy selects CDC.
- lightweight detection must not mount/export MSC media, relinquish flash ownership, run SCSI/block traffic, or expose host writes before installer/export entry.
- MSC entry must quiesce runtime/storage clients before host ownership of the staging/export FAT volume.
- the active USB personality may re-enumerate when switching from lightweight detection to MSC.
- exact pre-MSC descriptors and USBX implementation detail belong to the USB owner implementation and validation, not package/runtime code.

## USB Personalities

PeepShow v1 uses mutually exclusive USB personalities.

| Personality | Interface | Storage Owner | Purpose |
|---|---|---|---|
| normal installer/export | MSC only | host owns staging/export while mounted | universal package copy and artifact export |
| developer console | CDC only | firmware / `thStorage` | structured package upload, live-safe Platform tuning, telemetry, and capture commands |

Rules:

- VBUS attach alone must not enter or prompt for MSC installer/export behavior.
- boot USB parking is not a USB personality and not data-host detection; it is firmware-owned cleanup of PCD/USB clock/VDDUSB/HSI48 state before normal runtime.
- flash/staging provisioning is not MSC entry; it is an explicit `thStorage` maintenance command that may erase and format only the USB staging/export region. HW6 evidence `EV-HW6-20260812-P1-FLASHINIT-040` validates this as a destructive provisioning path, not as a side effect of MSC export.
- after USB data-host detection/enumeration and explicit installer/export entry, normal user transfer uses the MSC personality unless explicit dev-mode entry policy is active.
- CDC developer mode must not expose MSC at the same time in v1.
- CDC package upload writes through firmware-owned staging and `thStorage`, not a host-mounted FAT volume.
- CDC live tuning uses owner-routed, typed, validated Platform knob commands; it must not write raw memory directly.
- composite `MSC + CDC` is future work and requires new validation before use.

## Installer Mode Behavior

Installer/export mode is mostly unusable by design.

Allowed behavior:

- static Sharp Memory LCD status screen
- minimal input monitoring, especially `B` to exit/cancel when safe
- USB MSC host transfer
- no CDC developer control unless the active personality is explicitly developer CDC mode
- storage owner staging/rescan/install flow
- diagnostics explicitly allowed by policy

Disallowed behavior:

- active gameplay
- normal runtime audio
- non-installer storage clients
- host and MCU writing the same FAT/staging region simultaneously

## Platform Update Boundary

Package install and Platform firmware update are separate.

Normal package install consumes `PeepPkg` package data and commits validated package/content blobs and indexes.

Future Platform firmware update consumes a distinct Platform update artifact and must enter Platform-owned update/recovery policy before any firmware image is applied.

Rules:

- a Platform update artifact may arrive through MSC staging/export after host release.
- a Platform update artifact may arrive through CDC developer upload if developer mode supports that command family.
- both transfer paths must feed the same Platform-owned validation and apply flow where both are supported.
- `thStorage` may store or stage the artifact, but it does not decide firmware trust policy.
- package manager validation must reject Platform update artifacts as packages.
- the update flow must preserve or document recovery from failed update before production use.
- exact bootloader layout, update-slot strategy, signature enforcement, and protection settings are deferred until flash layout and Platform lifecycle are stable.

## Save and Settings Rules

### Joystick Calibration Records

The protected calibration region begins at `0x00020000` and is not
host-exposed. FW0 uses its first two 4 KiB sectors as calibration records A and
B; the rest of the 64 KiB region remains reserved for later Platform
calibration records.

- each record contains an explicit little-endian format version, generation,
  fixed joystick payload, CRC32, reserved bytes, and a separate commit marker
- `thStorage` is the only thread that scans, erases, programs, verifies, or
  commits these records
- a save preserves the selected record, erases and verifies the alternate
  sector, programs and verifies the body, writes the commit marker last, then
  rescans before publication
- boot selects the newest valid generation using wrap-safe comparison; an
  erased region produces no selection, while same-generation records with
  different payloads are a conflict and are not overwritten automatically
- normal joystick input is gated until boot resolution completes; no valid
  selection routes the shell to button-navigable joystick calibration

- Package saves and package-owned settings use [[Package_Save_Settings_API_Contract]].
- settings writes must be power-fail safe
- BLE pairing/bonding records must be power-fail safe and preserve the last valid record on failed update
- persistent fault-log writes must preserve the previous valid record on failed update
- joystick calibration must be available before normal shell/game input is considered usable
- save schema versions must support migration paths
- write frequency assumptions and wear strategy must be documented
- save/settings regions are not host-writable
- package-owned settings are separate from Platform settings/config

## Failure Policy

Storage failure is platform-critical.

If settings/calibration/storage cannot be validated:

- normal shell must not start
- route to `STORAGE_SAFE_MODE`
- expose diagnostics or USB recovery if safe
- never allow gameplay/runtime launch

If external flash is unavailable:

- package/runtime assets are unavailable
- saves/settings may be unavailable
- install/update is unavailable
- safe mode is required

## Validation Cases

1. flash probe/config/read/program/erase succeeds with bounded timing
2. flash deep power-down and wake/revalidate path works
3. local mount reaches `STORAGE_LOCAL_READY`
4. storage failure routes to `STORAGE_SAFE_MODE`
5. VBUS-only charger/power attach does not prompt for or enter MSC mode
6. USB data-host activity/enumeration is detected before MSC availability is offered; normal boot USB parking is explicitly not MSC availability
7. USB MSC exports only an already-provisioned staging/export volume and never auto-formats it during export
7a. reset or fresh boot uses non-destructive storage attach/check to restore flash-ready state without erasing or formatting
8. settings/saves/installed blobs are never host-writable
9. host write/read/delete smoke succeeds on staging/export volume; HW6 evidence `EV-HW6-20260812-P1-MSCSMOKE-041` validates create/read persistence across eject/reclaim/export and delete followed by clean reclaim on the freshly provisioned staging volume
10. firmware reclaim safely returns staging ownership to firmware; any staging classifier is diagnostic/package-browser input and must not auto-install or auto-launch on MSC exit
11. package install preserves last known valid installed index on interruption
12. runtime asset reads use [[Package_Asset_Loading_API_Contract]] over raw installed blob storage, not FAT/FileX
13. installer/export mode keeps display static and only minimal input active
14. logs/screenshots/debug exports are copied into staging/export without exposing internal regions directly
15. persistent fault-log ring preserves previous valid records and is not host-exposed
16. v1 USB personalities are mutually exclusive: MSC mode exposes no CDC developer control, and CDC developer mode exposes no MSC staging volume
17. CDC package upload routes through firmware-owned staging and package validation
18. package install rejects Platform update artifacts and never rewrites Platform firmware regions
19. future Platform update staging, if implemented, routes through Platform-owned update validation rather than package-manager commit

Related:

- [[Storage_Index]]
- [[USB_Development_Mode_Contract]]
- [[Package_Manager_State_Machine]]
- [[USB_MSC_Bring-up_and_Recovery_Runbook]]
- [[HW6_DMA_Map]]
- [[HW6_Pin_Ownership_Matrix]]
- [[Power_and_Sleep_Policy]]
