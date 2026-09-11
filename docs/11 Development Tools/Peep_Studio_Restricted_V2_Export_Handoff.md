# Restricted V2 Export Handoff

Status: backend implemented in authoring service API 42; Studio integration and
the normal Studio-build/USB-install round trip remain pending.

This increment enables a conservative export subset of the installed HW6 V2
profile. It supersedes the earlier blanket V2 export prohibition, not the
remaining firmware limitations. No firmware, wire IDs, source schema, fixture
or canonical target-profile constants change in this increment.

## Evidence And Scope

The unchanged GUI fixture `examples/authoring/native_v2_installation.peepproj`
from `e7f11f011fcbd91001aac0d15a85543c6212f3ab` already passed installed boot,
reinstallation/PLAY, A/B animation continuity and sleep-time reconciliation.
See [[V2_Installed_Package_Test_Runbook]]. Ordinary service build now produces
the same 2196 bytes as the explicit development encoder for that fixture.
This is not a shipping release, general V2 support or a measured-current claim.

## Capability Contract

Discover support from `service.hello`, not just `api_version`:

- `package_export.operation`: `project.build_package`.
- `package_export.container_versions`: `[1, 2]`.
- `package_export.v2_profile.profile_id`: `hw6_v2_resident_v1`.
- The profile supplies `limits`, supported event/interaction classes, timing
  and budget methods, and explicit unsupported-feature flags.
- `scene_object_authoring.status`: `restricted_firmware_available`.
- `scene_object_authoring.egg_export` and `firmware_available`: true.
- `scene_object_authoring.export_requires_project_readiness`: true.

Each scene capability response includes `egg_export`, `export_ready`,
`export_profile_id` and `export_readiness_scope: whole_project`.
`egg_export` means the backend supports the operation; it does NOT mean this
project is ready. A mixed project or unsupported scene blocks the whole build.
Use source `issues` for invalid drafts and `build_issues` for export blockers.
Saving, preview, undo/redo and existing authoring commands remain available for
valid drafts outside the export subset. Migration remains separate.

Use the existing revision-checked `project.build_package` operation. Its package
result retains the existing base64 bytes, byte count and hash and adds
`container_version` and `export_profile_id` (null for V1). Build the current
revision before exporting; never silently export the previous successful build
after a failed or stale build. Public `parse_egg` accepts this same restricted V2
subset; explicit development parsing remains broader for diagnostic fixtures.

## Export Subset

- Development build only; exactly one scene using scene-owned objects.
- Entire egg resident, at most 65536 bytes. This is not the 5 MiB storage limit.
- Continuous interaction; local input and scoped timer bindings.
- Object operations, variable writes, guards and start/restart/cancel timers.
- No audio assets/cues/chunks, scene exits/connections, shell-exit or explicit
  render-request actions, mixed V1/V2 projects or shipping builds.
- At most 12 objects, 8 states, 8 variables, 16 bindings, 16 expanded
  transitions, 16 guards, 32 actions and 256 strings.
- At most 8 animated objects; each clip has at most 12 steps and 4 distinct
  frames. Hidden objects and statically masked clips still count.

Transitions count once per source state; a scene handler counts once. Actions
and guards count the encoded shared records, not their expanded references.
The service returns actionable `V2_*` build issues rather than silently dropping
content, changing timing or falling back to a legacy execution model.

## Timing And Payload Budget

The firmware still derives its common animation interval. No authored scene-rate
field is introduced here. Frame durations must be whole 10 ms ticks and at most
2000 ms per frame. The latter conservatively fits the current 4 MHz MSIK,
divide-by-128, 16-bit timer path even when only one clip remains visible. Longer
holds can use repeated frames within the existing step limits. No rounding.

The backend derives the GCD of frame durations and LCM of complete clip cycles.
It bounds all authored clips together, including hidden and masked clips. It
also unions the display bands occupied at every authored horizontal position;
relative horizontal movement admits the whole clamped horizontal travel range.
This deliberately overestimates mutually exclusive states and ignores pixel
coincidences, occlusion and identical-payload reuse.

Limits remain 12 combined steps, 18 chunks and 10512 allocated payload bytes.
Each conservatively counted band slot uses 584 bytes. The report exposes:

- `budgets.waiting_visual_sequences.status`: `passed_conservative` or `blocked`.
- `analysis.quantum_ms`, `cycle_ms`, `combined_steps` and `panel_bands`.
- `analysis.chunks_upper_bound` and `payload_bytes_upper_bound`.
- `exact_device_admission_required: true`.

An awkward combination such as 250 ms and 400 ms clips may exceed these budgets
even though each clip fits alone. Studio should show the backend result and
suggest aligned timings, narrower animated content or restricted horizontal
movement. Do not implement another scheduler or resource calculator in the GUI.
This conservative subset can reject a scene that exact pixel packing could fit;
that is intentional for this first export increment.

Firmware still performs exact scheduling, raster and payload admission before
installation/activation and each staged transaction. Host readiness does not
replace those checks or prove physical playback, input response or low current.

## GUI Next Checkpoint

1. Merge the API 42 backend and discover the delivered capabilities.
2. Enable normal Build/Export for ready projects in this subset; present
   `build_issues` for blocked drafts without disabling their supported editing.
3. Build/export the existing installation fixture unchanged through Studio.
4. Send OS the exported egg path and GUI commit. OS will exercise normal USB
   scan/install, PLAY and intentional reboot, then verify animation/timer/input
   behavior on the installed bytes.

No replacement fixture, GUI-side compiler or firmware edits are requested.

## Verification

The authoring suite passes 315 tests, including native C profile/candidate/runtime
checks. Tests cover public service output matching the hardware fixture,
revision checks, deterministic bytes, unsupported drafts, capacity boundaries,
timing rejection, hidden clips and conservative motion/payload budgets.
The canonical target-profile generation check passes. No firmware source or
generated embedded egg changed, so no full firmware rebuild was performed for
this host-only increment. The Studio-export hardware round trip remains pending.
