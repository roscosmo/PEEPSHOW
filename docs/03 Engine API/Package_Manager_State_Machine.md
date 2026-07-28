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
- asset requests are legal only for the active package/runtime-unit context.
- required runtime-unit assets must be validated/prepared before runtime start.
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
6. asset request from inactive package/runtime unit is rejected
7. package quarantine invalidates outstanding asset handles
