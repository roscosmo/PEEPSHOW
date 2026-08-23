# Package Manager State Machine

This document defines explicit state machines for package catalog/index handling and package install/activation.

---

## Scope

Defines:
- package catalog and index readiness FSM
- install transaction FSM
- package activation FSM
- package asset request FSM summary

Does not define:
- installer transport ownership details (see [[Storage_and_Installer_Contract]])
- detailed asset handle/view API (see [[Package_Asset_Loading_API_Contract]])

---

## 1) Catalog and Index FSM

States:
- `PKG_INDEX_UNINITIALIZED`
- `PKG_INDEX_LOADING`
- `PKG_INDEX_READY`
- `PKG_INDEX_REBUILDING`
- `PKG_INDEX_ERROR`

Key events:
- `EV_BOOT`
- `EV_INDEX_LOAD_OK`
- `EV_INDEX_LOAD_FAIL`
- `EV_INDEX_REBUILD_REQ`
- `EV_INDEX_REBUILD_OK`
- `EV_INDEX_REBUILD_FAIL`
- `EV_INDEX_RECOVER_OK`

Rules:
- Package selection is illegal unless state is `PKG_INDEX_READY`.
- Rebuild operations must be bounded and fault-visible.

---

## 2) Install Transaction FSM

States:
- `PKG_INSTALL_IDLE`
- `PKG_INSTALL_STAGE`
- `PKG_INSTALL_VALIDATE`
- `PKG_INSTALL_COMMIT`
- `PKG_INSTALL_INDEX_UPDATE`
- `PKG_INSTALL_DONE`
- `PKG_INSTALL_ROLLBACK`
- `PKG_INSTALL_ERROR`

Key events:
- `EV_INSTALL_REQUEST`
- `EV_STAGE_OK`
- `EV_STAGE_FAIL`
- `EV_VALIDATE_OK`
- `EV_VALIDATE_FAIL`
- `EV_COMMIT_OK`
- `EV_COMMIT_FAIL`
- `EV_INDEX_UPDATE_OK`
- `EV_INDEX_UPDATE_FAIL`
- `EV_ROLLBACK_OK`
- `EV_ROLLBACK_FAIL`

Rules:
- Install state machine must preserve last known valid package set on failure.
- Commit and index update are not complete until both success events are observed.

---

## 3) Package Activation FSM

States:
- `PKG_ACTIVE_NONE`
- `PKG_ACTIVE_SELECTING`
- `PKG_ACTIVE_PREPARE_RUNTIME`
- `PKG_ACTIVE_RUNNING`
- `PKG_ACTIVE_SUSPENDED`
- `PKG_ACTIVE_RETURNING`
- `PKG_ACTIVE_ERROR`

Key events:
- `EV_PACKAGE_SELECTED`
- `EV_RUNTIME_PREPARE_OK`
- `EV_RUNTIME_PREPARE_FAIL`
- `EV_RUNTIME_STARTED`
- `EV_RUNTIME_SUSPENDED`
- `EV_RUNTIME_RESUMED`
- `EV_RUNTIME_EXITED`
- `EV_PACKAGE_FAULT`

Rules:
- Activation state must remain aligned with runtime manager state.
- Any mismatch routes to `PKG_ACTIVE_ERROR` and safe return to shell path.

### Current HW6 Bring-Up Boundary

HW6 firmware now resolves package activation through an explicit immutable
package-source view before invoking a scene loader.

Current source states are:

- `NONE`: no package blob is exposed.
- `EMBEDDED`: the generated development `.egg` blob is exposed.
- `STAGED_RAM`: one complete, volatile `.egg` copied from FileX by
  `thStorage` is exposed after every storage handle has closed.

Rules:

- `STAGED_RAM` is preferred when a completed staged image is available;
  otherwise `EMBEDDED` remains the development default until installed
  raw-blob storage is implemented.
- `NONE` is a normal `PKG_ACTIVE_NONE` result, not a package fault.
- a `NONE` activation request releases requested runtime clocks, returns to
  `SHELL / REACTIVE / RUNNING`, and presents `EGGLESS` on HOME.
- package loaders consume only the resolved immutable blob view; they do not
  know whether the source is embedded, staged in RAM, or installed.
- no activation path may read FileX or the staging filesystem.
- staged RAM publication proves a complete bounded copy and closed storage,
  not package validity. `thRuntime` must complete SHA-256, container, chunk,
  and scene-schema validation before changing `PKG_ACTIVE_NONE` to an active
  package state.
- the single staged RAM image is immutable for the active package lifetime;
  installer admission must exit that package before replacement begins.
- persistent installed-blob selection must not be added until the external
  flash package region, index, and atomic commit format are defined.

Verified HW6 evidence:

- embedded STATE activation remains functional, including STOP2 waiting
  visuals and return to HOME.
- one `.egg` transferred through MSC can be reclaimed, copied completely into
  the fixed staged-RAM source by `thStorage`, validated by `thRuntime`, and
  activated as the package-driven STATE scene. The package can return to HOME,
  and the system can repeatedly enter and wake from STOP2 afterward.
- forced `NONE` selection reports no active scene, `TX_NO_INSTANCE`, zero
  runtime capabilities, HOME with `EGGLESS`, and a responsive shell.

---

## 4) Asset Request FSM Summary

Detailed asset API rules are defined in [[Package_Asset_Loading_API_Contract]].

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
- asset requests are legal only for the active package/scene context.
- required scene assets must be validated/prepared before scene start.
- runtime hosts receive handles/views, not chunk offsets or storage addresses.
- storage reads are issued through the storage owner.
- asset faults propagate to runtime fallback, safe return, or package quarantine according to fault class.

---

## Required Integration

- Package manager transitions are owned by runtime/storage coordination layer.
- Storage owner performs data operations; package manager tracks logical transaction state.
- Index writes and commits must emit auditable transition logs.
- Asset loading uses [[Package_Asset_Loading_API_Contract]] and must remain aligned with package activation state.

---

## Validation Cases

1. clean install and activation path
2. failure at each install stage preserves last known valid state
3. index rebuild and recovery path correctness
4. runtime launch and return synchronization
5. invalid transitions rejected with explicit errors
6. asset request from inactive package/scene is rejected
7. package quarantine invalidates outstanding asset handles
