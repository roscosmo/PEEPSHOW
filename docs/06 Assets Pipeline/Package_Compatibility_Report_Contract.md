# Package Compatibility Report Contract

This document defines the compatibility report produced by PeepOS package tooling.

The compatibility report is the bridge between a package, a selected target profile, the capability registry, package schemas, and the build profile used for export.

It is diagnostic and reproducibility metadata. It does not grant capabilities, mutate target profiles, or replace firmware install validation.

Related:

- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Package_Blob_Format_Contract]]
- [[Package_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Game_Authoring_API_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Evidence_Artifact_Convention]]

---

## Purpose

Every package build must produce a compatibility report.

The report answers:

- which package was built
- which target profile it was checked against
- which schemas and tools produced the result
- which runtime units exist
- which templates, Authoring Kits, prefabs, behavior graphs, or behavior macros contributed source content where recorded
- which capabilities are required, optional, granted, blocked, mocked, or waived
- which budgets were consumed
- which validation results block export or remain as warnings
- whether the report is valid for shipping, development, preview, or digital twin use

The report is also the primary artifact used by authoring tools to explain rejection reasons in PeepOS terms.

---

## Boundary

The compatibility report may contain:

- package identity and checksums
- selected build profile
- selected target profile identity, version, status, and hash
- tool and schema versions
- runtime unit inventory
- authoring source provenance for templates, Authoring Kits, prefabs, behavior graphs, and behavior macros
- capability admission results
- budget summaries
- validation issues
- waivers
- artifact references

The compatibility report must not contain:

- HAL, LL, register, DMA, clock, pin, or RTOS object names
- filesystem paths needed by runtime behavior
- flash offsets, erase pages, raw storage regions, SRAM bank names, linker sections, heap regions, or DMA buffer addresses
- debug transport ownership
- Platform knob values that are not published through target profiles
- host machine secrets or local absolute paths

Internal verifier logs may contain lower-level toolchain debugging detail, but those logs are not the normal package compatibility report shown to game authors.

---

## Report Status

Allowed report statuses:

| Status | Meaning |
|---|---|
| `passed` | package is compatible with the selected target/build profile |
| `passed_with_waivers` | compatible only because allowed warnings were explicitly waived |
| `preview_only` | usable for authoring preview or host mocks, not dev-device or shipping export |
| `dev_only` | usable for development target profiles, not shipping export |
| `failed` | package cannot be built/exported for the selected target/build profile |
| `stale` | report was valid when produced but no longer matches source hashes, profile hash, schema versions, or package checksum |

Shipping export requires `passed` or an explicitly release-policy-allowed `passed_with_waivers`.

---

## Required Schema

```text
compatibility_report:
  report_schema_version
  report_id
  report_status
  build_profile                 # authoring_preview, dev_package, release_candidate, shipping
  generated_by:
    tool_name
    tool_version
    validator_version
    package_compiler_version
    schema_versions[]
  package:
    package_id
    package_name
    package_version
    package_container_version
    package_checksum
    source_manifest_checksum
    content_parameter_checksum
  authoring_source:
    templates[]:
      template_id
      template_name
      template_version
      source_status              # imported, customized, stale, unresolved
      editable_slot_status
      generated_runtime_unit_refs[]
      issue_refs[]
    authoring_kits[]:
      kit_id
      kit_name
      kit_version
      kit_type
      generated_runtime_unit_refs[]
      required_capabilities[]
      optional_capabilities[]
      issue_refs[]
    prefabs[]:
      prefab_id
      prefab_name
      prefab_version
      scene_refs[]
      issue_refs[]
    behavior_graphs[]
    behavior_macros[]
  target_profile:
    profile_id
    profile_version
    profile_status
    profile_hash
    source_evidence_refs[]
  capability_registry:
    registry_version
    registry_hash
  result_summary:
    fatal_count
    error_count
    warning_count
    advisory_count
    waived_count
    blocking_count
  runtime_units[]:
    unit_id
    unit_name
    runtime_class
    entry_ref
    reactive_fallback_ref
    required_capabilities[]
    optional_capabilities[]
    budget_status
    validation_status
  capabilities[]:
    capability
    requested_by[]              # package, runtime_unit, asset, schedule, context
    requirement_level           # required, optional, session_required, dev_only
    target_grant_status
    fallback_declared
    admission_status            # granted, clamped, optional_with_fallback, blocked, mocked, pending_validation
    issue_refs[]
  budgets:
    runtime_logic
    rendering
    assets
    package_size
    runtime_ram
    save_settings
    diagnostics
    input
    sensors
    audio
    communication
    time_power
    waiting_visual_sequences
  input_lock:
    enabled
    target_timeout_policy_ref
    meaningful_activity_sources[]
    lock_route
    lock_target
    locked_waiting_visual_ref
    bounded_deferral_status
    contexts[]:
      state_path
      meaningful_activity_sources[]
      bounded_deferral_status
    unlock_action = START
    unlock_press_consumed = true
    admission_status
    issue_refs[]
  waiting_visual_admission[]:
    state_path
    preferred_visual_ref
    fallback_visual_ref
    compiler_profile_id
    admission_status            # admitted, fallback_selected, blocked, pending_validation
    abstract_utilization        # normalized percentage or profile-defined class; no row/chunk details
    selected_result
    issue_refs[]
  validation_results[]:
    issue_id
    severity
    category
    package_path
    user_message
    internal_detail_ref
    blocks_authoring_preview
    blocks_dev_package
    blocks_shipping_package
    waiver_ref
  waivers[]:
    waiver_id
    severity
    issue_ref
    reason
    owner
    expiry_or_removal_condition
    allowed_build_profiles[]
  artifacts:
    package_blob_ref
    package_blob_checksum
    compatibility_report_checksum
    deterministic_build_ref
    validation_log_ref
```

`package_path` is a package/schema path such as a runtime unit, asset ID, schedule ID, or content parameter ID. It must not be a host absolute filesystem path.

---

## Validation Categories

Compatibility report categories:

| Category | Examples |
|---|---|
| `manifest` | package identity, entry point, schema version |
| `authoring_source` | template, Authoring Kit, prefab, graph, or macro provenance and customization status |
| `runtime_units` | runtime class, declared transitions, fallback route |
| `runtime_logic` | states, guards, actions, queues, bounded execution |
| `capabilities` | required/optional grants, fallback behavior |
| `power_time` | reactive waits, optional input lock/routes, cadence, schedules, calendar use |
| `rendering` | logical surface, layers, tone5, waiting-visual assets and fallbacks |
| `assets` | chunk bounds, decode budget, deterministic conversion |
| `input` | logical input bindings, focus scopes, wake intents |
| `sensors` | sensor contexts, cadence, wake capability |
| `audio` | cues, voices, format, mute policy |
| `communication` | sessions, message schemas, rates, wake restrictions |
| `save_settings` | schema, migration, write policy, persistence results |
| `diagnostics` | marker/counter/timing limits, shipping policy |
| `package_limits` | size, RAM, string, content parameter, diagnostic budgets |
| `internal_safety` | forbidden internal symbols in generated artifacts |
| `determinism` | reproducible output, stable ordering, checksum mismatch |

User-facing messages must use PeepOS package language. Low-level internal terms belong in `internal_detail_ref`, not in normal authoring messages.

---

## Severity Model

The report uses the severity model from [[Game_Authoring_API_Contract]]:

| Severity | Blocks Shipping |
|---|---:|
| `fatal` | yes |
| `error` | yes |
| `warning` | yes unless waived by release policy |
| `advisory` | no |
| `waived` | no if release policy allows the waiver |

Warnings that affect runtime safety, determinism, storage integrity, power policy, or capability availability must escalate to `error`.

---

## Capability Admission

Capability admission compares package declarations with the selected target profile.

Rules:

- required capabilities must be granted by the target profile.
- optional capabilities require declared fallback behavior.
- session-required communication or audio-centric behavior is valid only when explicitly declared and bounded.
- `pending_validation` grants block shipping export.
- mocked capabilities are preview/dev-only and must not appear as shipping-compatible.
- development-only inputs or capabilities must be stripped, nulled, or rejected on non-dev target profiles.
- a package may carry waiting-visual assets on any compatible profile; continued motion while reactive logic is yielded requires `display.waiting_visual_animation`, or the package must admit a reduced/hold fallback.

Capability names must come from [[PeepOS_Capability_Registry]].

---

## Budget Reporting

Budget summaries must include:

- target limit
- package requested/used value
- unit of measure
- status
- source path or package ID

Budget entries must use target-profile abstractions such as package bytes, asset bytes, runtime RAM bytes, save/settings bytes, frame budget, message size, event rate, or waiting-visual sequence bytes.

Budget entries must not expose memory-map or hardware-transfer facts. Waiting-visual admission reports only normalized utilization or a profile-defined complexity class; row spans, transfer chunks, descriptor counts, SRAM placement, and wire payload sizes remain protected Platform compiler diagnostics.

---

## Waivers

Waivers are allowed only for warnings or advisories unless a special development profile explicitly permits a preview-only mock.

Rules:

- waivers must have reason, owner, and removal condition.
- waivers must list allowed build profiles.
- waivers must be included in the report.
- waivers must not hide fatal, schema, integrity, deterministic-build, required-capability, power-policy, or runtime-safety failures in shipping exports.
- waivers used for preview or mocked behavior are not hardware bring-up evidence.

---

## Determinism And Staleness

The report must become stale if any of these change:

- package checksum
- source manifest checksum
- template, Authoring Kit, prefab, behavior graph, or behavior macro source hash where recorded
- selected target profile hash
- capability registry hash
- package schema version
- asset compiler version where output semantics change
- content parameter checksum
- selected build profile
- waiver set

Reports retained inside deterministic package payloads must not include hidden timestamps or host-local paths.

If a human-readable sidecar includes generation time, that timestamp must be outside the deterministic package checksum or otherwise explicitly excluded from deterministic rebuild comparison.

---

## Package Embedding

The package blob may include a `compat_report` chunk for diagnostics.

Rules:

- installer validation must not trust the report as proof of package safety.
- installer validation must re-check package integrity, schema compatibility, runtime class compatibility, and required capability compatibility.
- a report embedded in a package must match the package checksum or be ignored as stale.
- a sidecar report may include richer authoring detail than an embedded report.

---

## Digital Twin Use

The digital twin may use compatibility reports to:

- choose the correct target/twin profile
- reject stale or incompatible packages before launch
- show authoring diagnostics
- seed replay manifests with package/profile/report hashes
- explain mocked or preview-only capabilities

The digital twin must still enforce runtime contracts directly. A compatibility report does not bypass runtime validation.

---

## Required Validation Cases

1. package requiring a blocked capability produces `failed`.
2. package using optional capability without fallback produces `failed`.
3. package using a pending-validation capability produces `preview_only` or `dev_only`, not shipping-compatible.
4. package exceeding any target-profile budget produces a blocking issue.
5. package with allowed waived warning produces `passed_with_waivers`.
6. package with waived runtime-safety error still fails shipping export.
7. package report becomes stale after target profile hash changes.
8. package report becomes stale after package checksum changes.
9. embedded report mismatch is ignored by installer diagnostics and does not affect installer validation authority.
10. user-facing messages do not expose HAL, RTOS, filesystem, flash-offset, SRAM, DMA, register, or debug-transport terms during normal authoring.
11. compatibility report contains no host absolute paths required by runtime behavior.
12. digital twin rejects a package whose report targets a different source target profile unless explicitly revalidated.
13. package with automatic locking disabled validates without a lock route.
14. enabled input lock without one admitted route fails validation.
15. package-authored lock timeout or unbounded lock deferral fails validation.
16. compatibility output records Start as consumed system unlock and never as a simultaneous package action.

---

## Rule

The compatibility report records the result of package validation against a target profile.

It is evidence of tool-side validation, not a substitute for firmware install validation or hardware bring-up evidence.
