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

External `AT25SL128A` regions must be explicitly defined before firmware implementation.

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
- `thStorage` owns FileX/LevelX, the MSC media bridge, and export/reclaim sequencing, but USB active clock selection is requested through `thPower`
- export prep must not use detached USB-park checks that reject VBUS; detached parking and active MSC export are different states
- reclaim disables the bridge export policy, stops/disconnects USB, closes FileX/LevelX, returns the clock policy to base, then firmware may rescan staging/export

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
6. USB data-host activity/enumeration is detected before MSC availability is offered
7. USB MSC exports only staging/export volume
8. settings/saves/installed blobs are never host-writable
9. host write/read/delete smoke succeeds on staging/export volume
10. firmware reclaim/rescan detects changed staging contents
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
