# Package Save And Settings API Contract

This document defines the Engine-side API model for package-owned persistent state.

It covers package save records and package-owned settings. It does not cover Platform settings, calibration, BLE bonding, fault logs, install indexes, or hardware policy records.

---

## Boundary

Packages may use:

- schema-versioned save records
- package-owned settings declared by schema
- explicit migrations
- explicit reset flows
- bounded write requests
- read-only metadata about save/settings availability

Packages must not use:

- filesystem paths
- FAT/FileX records
- raw flash offsets
- Platform settings records
- calibration records
- BLE bonding records
- fault-log records
- storage wear-policy controls
- direct erase/program operations

The Engine owns package save/settings APIs. The Platform storage owner performs physical persistence.

---

## Core Rules

- Saves and package settings are records, not files.
- Every write targets a declared schema record.
- Every record has version, size, default, migration, and reset policy.
- Writes are bounded and may be delayed, clamped, coalesced, rejected, or retried by policy.
- Failed writes must preserve the previous valid record where possible.
- Package save/settings data is not host-writable staging content.
- Package code must handle read/write failure.
- Package code must not assume a write reaches flash immediately.
- Platform settings are not package settings.

---

## Data Classes

| Data Class | Owner | Package Access |
|---|---|---|
| package save records | package through Engine API | read/write by declared schema |
| package-owned settings | package through Engine API and optional PeepOS UI | read/write by declared schema |
| Platform settings | PeepOS Platform | no direct package write |
| calibration | Platform/input/sensor owners | no package access except normalized APIs |
| BLE bonding | `thComm`/`thStorage` | no package access |
| persistent fault log | fault supervisor/`thStorage` | diagnostics only where allowed |
| installed package index | package manager/`thStorage` | query only through package metadata APIs |

Package-owned settings include preferences such as difficulty, text speed, local sound preference, package-local input preference, or package-local accessibility preference.

Content parameters are package-authored balancing or behavior values. If a content parameter needs runtime/user persistence, it must be represented as a package-owned setting or save record. It must not be promoted into a Platform knob or Platform setting.

Hardware-affecting behavior is expressed through bounded capability contexts, not settings mutation.

---

## Save Schema

Conceptual schema:

```text
save_schema:
  save_schema_id
  save_schema_version
  records[]:
    record_id
    record_type
    fields[]
    max_size_bytes
    default_value
    migration_policy
    reset_policy
    write_policy
    durability_class
  package_settings[]:
    setting_id
    value_type
    default_value
    allowed_values
    ui_metadata
    storage_record_ref
    migration_policy
  write_budget
  reset_policy
```

Rules:

- `record_id` and `setting_id` are stable symbolic IDs within the package.
- field types must be schema-defined.
- record size must be statically bounded.
- default values must validate against the schema.
- schema changes require versioning and explicit migration policy.
- removed records must define preserve, migrate, or reset behavior.
- package settings must map to package-owned storage records, not Platform settings.

---

## Record Types

Initial record types:

| Type | Use |
|---|---|
| `state_snapshot` | package-owned progress/state checkpoint |
| `counter` | bounded numeric counters |
| `flags` | compact boolean/progress flags |
| `inventory_table` | bounded package table |
| `settings_record` | package-owned settings values |
| `history_ring` | bounded package-local event/history ring |
| `opaque_bounded` | package-defined bytes with schema version and max size |

`opaque_bounded` is allowed only when tooling can validate size, version, default, migration, and write policy. It is not a filesystem escape hatch.

---

## Write Policy

Every writable record declares write policy.

```text
write_policy:
  max_record_bytes
  max_writes_per_period
  minimum_interval_ms
  coalesce_allowed
  write_on_suspend
  write_on_exit
  power_fail_strategy
  priority
```

Rules:

- high-frequency writes may be clamped or rejected.
- repeated writes may be coalesced where record policy allows it.
- `write_on_suspend` must finish, abort, or time out before sleep entry.
- writes inside realtime frame loops are invalid unless they are buffered/coalesced outside the frame path.
- write priority does not bypass storage owner policy.
- package logic must tolerate delayed commit.

---

## Durability Classes

| Class | Meaning |
|---|---|
| `volatile_runtime` | not persisted; runtime-only state |
| `fast_resume` | continuity across STOP-class sleep; not durable |
| `persistent_save` | durable package-owned save data |
| `persistent_setting` | durable package-owned setting |

`fast_resume` is not a substitute for flash-backed save data.

Packages must not choose memory banks. They declare durability intent; Engine/Platform choose placement.

---

## Suggested API Shape

This is a conceptual C-level shape. Final signatures may differ, but must preserve the bounded and schema-driven model.

```c
typedef enum {
    SAVE_OK = 0,
    SAVE_DEFERRED,
    SAVE_ERR_NOT_READY,
    SAVE_ERR_NOT_FOUND,
    SAVE_ERR_SCHEMA,
    SAVE_ERR_VERSION,
    SAVE_ERR_BOUNDS,
    SAVE_ERR_WRITE_BUDGET,
    SAVE_ERR_STORAGE,
    SAVE_ERR_MIGRATION,
    SAVE_ERR_REVOKED,
    SAVE_ERR_INTERNAL
} save_result_t;

typedef struct {
    uint32_t package_id;
    uint32_t runtime_unit_id;
    uint32_t record_id;
    uint32_t schema_version;
    uint32_t max_bytes;
    uint32_t timeout_ms;
} save_record_req_t;

typedef struct {
    uint32_t setting_id;
    uint32_t value_type;
    uint32_t schema_version;
    uint32_t flags;
} package_setting_desc_t;
```

Package-visible persistence result classes:

| Result | Meaning |
|---|---|
| `success` | record accepted and published by the save/settings contract |
| `deferred` | write accepted for later flush/coalescing where policy allows |
| `rejected_budget` | write exceeds rate, size, or lifecycle budget |
| `rejected_schema` | write does not match declared schema/version/bounds |
| `unavailable` | save/settings service is not available for this package/profile/state |
| `failed_preserved` | write failed but the previous valid record is preserved where required |

Package logic and authoring tools must model these results. The cause of a storage or hardware failure remains Platform diagnostics; packages see only the bounded persistence result.

Required API families:

- query save/settings availability
- query save schema version
- read save record by ID
- write save record by ID
- request write flush where allowed
- enumerate package-owned settings descriptors
- read package-owned setting
- write package-owned setting
- run approved migration
- reset package-owned save/settings data through approved flow
- inject/read error status for diagnostics and digital twin tests

---

## Migration Model

Migration is explicit and bounded.

Rules:

- package install or first launch may detect older save schema.
- migration paths must be declared in the package.
- migration code/data must be bounded by instruction, memory, and write limits.
- destructive migration requires explicit policy.
- failed migration must preserve the previous valid record where possible.
- missing required migration rejects package activation or routes to reset prompt according to package policy.
- downgrades are unsupported unless explicitly declared.

Migration outcomes:

| Outcome | Meaning |
|---|---|
| `migrated` | old record converted to current schema |
| `preserved_old` | old record kept; package cannot use new schema until resolved |
| `reset_to_default` | record reset through declared policy |
| `migration_failed` | activation blocked or package enters recovery/fallback |

---

## Reset And Erase

Package-owned reset flows must be explicit.

Allowed reset targets:

- one save record
- package-owned settings
- all package save data
- all package-owned persistent data

Rules:

- reset must not erase Platform settings, calibration, BLE bonding, package install metadata, or other packages' data.
- user/system confirmation policy is PeepOS-owned.
- package may request reset through Engine API, but Platform/Engine approve and execute it.
- reset must preserve unrelated valid records where possible.

---

## Transaction And Failure Policy

Save writes are transactional from the package perspective.

Required behavior:

- validate schema and bounds before write.
- write new record without corrupting previous valid record.
- verify integrity metadata before publishing success.
- on failure, return explicit error and preserve previous valid record where possible.
- publish save/storage fault diagnostics at bounded rate.

Storage owner may use copy/swap, journal, ring, or another implementation. Package APIs expose only record-level success/failure.

---

## Suspend, Resume, And Power

Before sleep:

- in-flight writes must complete, abort, or time out.
- write-on-suspend records may be flushed if policy allows.
- noncritical writes may be deferred.
- package runtime must not block sleep indefinitely for save writes.

After resume:

- Engine revalidates save/settings availability as needed.
- package logic must handle delayed, failed, or deferred writes.
- `fast_resume` state may restore continuity but must be validated and fall back to durable save data or defaults if invalid.

---

## Package Settings UI

PeepOS may render package-owned settings using schema metadata.

Allowed metadata:

- label/string table reference
- value type
- allowed values/range
- default value
- visibility policy
- restart/reload requirement
- package-local accessibility hint

Rules:

- UI metadata must not include hardware controls.
- changing a package setting must use the package settings API.
- package settings may influence package logic only.
- hardware-affecting behavior is expressed through bounded capability contexts.

---

## Digital Twin Requirements

The digital twin must implement the same save/settings API semantics.

Required behavior:

- load initial save state from a compiled test fixture or generated default records.
- enforce schema, version, bounds, write budget, and migration rules.
- inject read failure, write failure, interrupted write, migration failure, and storage-unavailable cases.
- preserve previous valid record on simulated failed write where the contract requires it.
- report save/settings state changes in deterministic replay logs.
- never expose host filesystem paths as package runtime APIs.

Digital twin save evidence is package/Engine evidence only. It is not flash wear, power-fail, or storage-media bring-up evidence.

---

## Validation Cases

1. package without save schema cannot write save records.
2. write to undeclared record ID is rejected.
3. oversized record write is rejected before storage access.
4. high-frequency write exceeds budget and is clamped or rejected.
5. failed write preserves previous valid record where possible.
6. migration from old schema to current schema follows declared path.
7. missing required migration blocks activation or triggers declared reset flow.
8. package setting read/write uses package-owned schema and storage only.
9. package cannot mutate Platform settings through save/settings APIs.
10. reset package save data does not erase other packages or Platform records.
11. suspend with in-flight write completes, aborts, or times out before sleep.
12. digital twin injects write failure and package handles it through declared fallback.

---

Related:

- [[Package_Contract]]
- [[Content_Parameter_Schema_Contract]]
- [[Package_Blob_Format_Contract]]
- [[Package_Asset_Loading_API_Contract]]
- [[Storage_and_Installer_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Game_Authoring_API_Contract]]
