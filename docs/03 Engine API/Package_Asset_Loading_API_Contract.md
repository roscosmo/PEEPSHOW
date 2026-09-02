# Package Asset Loading API Contract

This document defines the Engine-side API model for querying packages and loading runtime assets from installed `PeepPkg` blobs.

It sits between runtime hosts and Platform storage.

Runtime hosts and packages use asset IDs, package metadata, typed asset views, and bounded handles. They do not use storage offsets, chunk table offsets, flash addresses, FileX paths, FAT paths, DMA buffers, or hardware payload formats.

---

## Boundary

The package manager owns:

- installed package index/catalog
- package activation state
- manifest and chunk-table validation results
- asset ID to chunk mapping
- scene asset admission
- package-safe asset handles
- cache/preload policy
- package quarantine and asset-handle invalidation

The Platform storage owner owns:

- external flash
- raw installed package storage
- protected firmware-update staging
- protected project-namespaced game data
- staging/export storage
- filesystem and flash translation layers
- physical read/program/erase operations

Runtime hosts may request package assets only through this contract.

---

## Single-Active-Package Lifecycle

The product has one active installed package, not an A/B runtime catalog. A
package handle is scoped to that active package identity and activation
generation; package replacement, package quarantine, or a pending install
transaction invalidates every outstanding handle.

The active package slot is `5 MiB`. It is not copied into a megabyte-scale RAM
cache. Package metadata may be prepared in fixed RAM, but large package content
is served by bounded `thStorage` reads into fixed owner-managed buffers.

The former B package region is not available to package handles. Its fixed
subregions are reserved for firmware-update staging and project-namespaced game
data. Package code cannot address either region directly. Game data must use a
separate Platform-owned data API; firmware-update staging is inaccessible to
packages and the normal package manager.

---

## Core Rules

- Assets are addressed by stable package asset IDs.
- Runtime may use compact handles returned by the package API.
- Runtime must not store or fabricate chunk offsets.
- Runtime must not read FAT/FileX paths during active package execution.
- Runtime must not parse editor-native files such as PNG, Aseprite, Tiled, WAV, JSON, or TMX.
- All reads, decodes, and cache fills are bounded.
- All asset handles are scoped to an active package and scene.
- Package update, uninstall, quarantine, or activation failure invalidates outstanding handles.
- Required scene assets must be validated before the scene starts.

---

## Asset Identity Model

Authoring and package metadata use stable symbolic asset IDs.

Runtime implementation may translate those IDs to compact numeric handles during package activation.

Conceptual identifiers:

```text
package_id
scene_id
asset_id
asset_type
asset_handle
asset_view
```

Rules:

- `asset_id` is unique within a package.
- `asset_id` is not a filesystem path.
- `asset_handle` is not persistent across package activation sessions.
- handles from one package must not be accepted by another package.
- handles from one scene may access only assets declared for that scene or approved shared assets.

---

## Asset Load Policies

Every asset record declares a load policy.

| Policy | Meaning |
|---|---|
| `preload_on_mount` | load/prepare before the scene starts |
| `scene_local` | available only while the declaring scene is active |
| `shared_resident` | may remain cached across scene transitions in the same package |
| `windowed_read` | read bounded regions on demand through package API |
| `stream_bounded` | sequential bounded reads through an owner-managed ring/buffer |
| `prepare_on_demand` | may be prepared lazily outside critical frame/event paths |
| `optional_fallback` | asset may be unavailable if declared fallback exists |

Rules:

- sequence/program frame loops may not trigger unbounded storage reads or decodes.
- low-power/runtime event handlers may not block on long asset loads.
- `preload_on_mount` failures reject scene start unless fallback is declared.
- optional assets must declare fallback behavior.
- streaming assets must declare maximum read size, buffer depth, and latency tolerance.

---

## Suggested API Shape

This is a conceptual C-level shape. Final signatures may differ, but must preserve the ownership and boundedness rules.

```c
typedef enum {
    PKG_OK = 0,
    PKG_ERR_NOT_READY,
    PKG_ERR_NOT_FOUND,
    PKG_ERR_TYPE,
    PKG_ERR_CAPABILITY,
    PKG_ERR_BOUNDS,
    PKG_ERR_TIMEOUT,
    PKG_ERR_INTEGRITY,
    PKG_ERR_REVOKED,
    PKG_ERR_INTERNAL
} pkg_result_t;

typedef enum {
    PKG_ASSET_MASKED_1BPP_SPRITE_BANK,
    PKG_ASSET_TONE5_SPRITE_BANK,
    PKG_ASSET_TILESET_BANK,
    PKG_ASSET_TILEMAP,
    PKG_ASSET_ANIMATION_TABLE,
    PKG_ASSET_FONT_BANK,
    PKG_ASSET_TEXT_TABLE,
    PKG_ASSET_AUDIO_BANK,
    PKG_ASSET_BBB_PATTERN_BANK,
    PKG_ASSET_WAITING_VISUAL_SEQUENCE,
    PKG_ASSET_STATE_GRAPH,
    PKG_ASSET_DATA_TABLE
} pkg_asset_type_t;

typedef struct {
    uint32_t package_id;
    uint32_t scene_id;
    uint32_t asset_id;
    pkg_asset_type_t expected_type;
    uint32_t timeout_ms;
} pkg_asset_open_req_t;

typedef struct {
    uint32_t handle_id;
    pkg_asset_type_t type;
    uint32_t byte_size;
    uint32_t flags;
} pkg_asset_handle_t;
```

Required API families:

- package metadata query
- package capability query
- asset metadata query by ID
- asset open/close by ID
- asset view acquisition for prepared assets
- bounded read-window request for large assets
- streaming request setup for approved streaming assets
- scene preload/prepare
- cache release and pressure notification
- handle invalidation notification

---

## Typed Asset Views

Package asset handles expose typed views, not raw arbitrary byte buffers.

Examples:

| Asset Type | Runtime View |
|---|---|
| `masked_1bpp_sprite_bank` | sprite records, mask plane metadata, bounds |
| `tone5_sprite_bank` | tone records, opacity metadata, scale limits, coverage rules |
| `tileset_bank` | tile records, tile size, pixel model, scale limits |
| `tilemap` | map dimensions, layer records, viewport/read windows, collision/data tables |
| `animation_table` | frame references, timing, loop policy |
| `font_bank` | glyph metrics, glyph bitmap refs, line metrics |
| `text_table` | string IDs, localized records, bounds |
| `audio_bank` | cue records, ADPCM block layout, stream budget |
| `waiting_visual_sequence` | bounded logical 1bpp visual states and timing policy |

Rules:

- typed views are validated before exposure.
- byte-order and alignment conversion is an Engine/package-loader concern.
- package code must not cast asset data to arbitrary structs outside the approved view.
- view lifetime ends when the handle is closed, revoked, or package is deactivated.
- an installed package larger than the resident prefix may expose only a final
  `audio_bank` through bounded storage-owner reads; all scene metadata and all
  non-audio assets remain resident before that bank.
- the installer validates the complete package. The partial loader validates
  the resident metadata and the decoder validates ADPCM block headers as source
  windows arrive; a package-backed audio view never grants direct flash access.

---

## Scene Preparation

Before a scene starts, the package manager must:

1. confirm the package index is ready.
2. confirm the package is installed and not quarantined.
3. validate the target scene exists.
4. validate compiler-derived required capabilities for that scene.
5. validate required assets for that scene.
6. preload or prepare assets marked `preload_on_mount`.
7. report optional assets that are unavailable and activate declared fallbacks.
8. hand a prepared package/scene context to the runtime host.

Sequence/program scenes should start only after frame-critical assets are prepared or an explicit loading/fallback route is declared.

---

## Asset Request FSM

Asset request handling is explicit and bounded.

States:

- `ASSET_REQ_IDLE`
- `ASSET_REQ_VALIDATE`
- `ASSET_REQ_RESOLVE_CHUNK`
- `ASSET_REQ_CHECK_INTEGRITY`
- `ASSET_REQ_READ`
- `ASSET_REQ_DECODE_PREPARE`
- `ASSET_REQ_READY`
- `ASSET_REQ_RELEASE`
- `ASSET_REQ_ERROR`

Rules:

- invalid asset IDs fail in `ASSET_REQ_VALIDATE`.
- undeclared scene assets fail before read.
- chunk integrity failure routes to package fault or optional fallback.
- reads are issued through the storage owner.
- decode/prepare has explicit budget.
- failed required assets prevent scene start.
- failed optional assets must activate declared fallback behavior.

---

## Cache And Memory Policy

Package asset cache policy must be deterministic.

Rules:

- cache budgets are declared per target profile.
- cache entries are owned by the package manager or approved Engine subsystem.
- packages do not allocate cache memory directly.
- cache pressure may evict non-pinned optional assets.
- pinned assets require declared lifetime and budget.
- cache eviction must not invalidate active typed views without notification.
- all cache fills and evictions are auditable in diagnostics.

Memory classes:

| Memory Class | Use |
|---|---|
| normal/retained runtime SRAM | prepared runtime views, working caches, renderer working planes, committed framebuffer, and fast-resume state |
| SRAM4 | Platform display-DMA TX scratch overlay, compiled waiting-visual payloads, descriptors, queue, metadata, and guard only |
| external flash | durable installed packages, saves, settings, logs |

Packages must not request memory banks by name.

---

## Streaming And Windowed Reads

Large assets may use bounded read windows.

Allowed examples:

- tilemap region/window reads
- audio block/ring-buffer reads
- large text table section reads
- optional diagnostic/export data reads outside active gameplay loops

Rules:

- request size is bounded.
- request timeout is explicit.
- storage owner may reject or delay during sleep/install/export/fault policy.
- `thAudio` requests approved audio windows or stream refills but does not read
  raw package storage itself.
- a sequence/program scene may not wait inside its frame budget on a storage read.
- streaming audio is owned by the audio subsystem and uses declared ring/buffer policy.
- windowed tilemap reads must have fallback behavior for cache miss or delayed read.

---

## Suspend, Resume, And Low Power

On suspend:

- runtime hosts must stop issuing new asset requests.
- package manager may release non-retained caches.
- in-flight storage reads must finish, abort, or time out before sleep entry.
- handle state must be made resumable or invalidated cleanly.

On resume:

- package manager revalidates package/index state.
- runtime host must tolerate cache misses for non-pinned assets.
- required handles must be reacquired or verified before use.
- failure routes to runtime fallback, shell return, or package quarantine according to fault class.

`waiting_visual_sequence` assets are portable package data. Platform display owner may compile separate SRAM4/LPBAM payloads where supported, but those payloads are not package asset handles.

---

## Fault And Quarantine Policy

Package asset failures map to explicit outcomes:

| Failure | Outcome |
|---|---|
| missing required asset | reject scene start |
| missing optional asset | activate fallback |
| chunk CRC failure | package fault; quarantine if required chunk |
| unsupported required asset version | reject package or scene |
| unsupported optional asset version | fallback if declared |
| storage unavailable | safe return or safe mode according to Platform policy |
| handle use after revoke | runtime fault and safe stop |

Package quarantine invalidates all package asset handles and prevents activation until recovery/reinstall.

---

## Digital Twin Requirements

The digital twin must load assets through the same package asset API semantics.

Rules:

- twin uses compiled `PeepPkg` data or a contract-equivalent compiled package representation.
- twin must not let host filesystem paths become package runtime APIs.
- twin must enforce handle lifetime, type checks, capability checks, scene asset scope, and fallback behavior.
- twin may inject asset read failures, cache misses, checksum failures, and unavailable optional assets.

---

## Validation Cases

1. scene starts only after required assets are validated/prepared.
2. duplicate or unresolved asset IDs fail package validation.
3. asset type mismatch is rejected before runtime use.
4. scene cannot open an asset not declared for that scene.
5. optional missing asset activates fallback.
6. required chunk CRC failure quarantines or rejects the package.
7. sequence/program frame loop does not block on storage reads.
8. suspend/resume invalidates or restores handles according to policy.
9. package uninstall/update/quarantine invalidates outstanding handles.
10. digital twin enforces the same handle and asset-scope rules.

---

Related:

- [[Package_Manager_State_Machine]]
- [[Package_Contract]]
- [[Package_Blob_Format_Contract]]
- [[Rendering_API_Contract]]
- [[Storage_and_Installer_Contract]]
- [[Runtime_Host_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
