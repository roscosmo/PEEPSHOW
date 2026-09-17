# Multi-scene V2 Export Handoff

Status: 2026-09-17, service API 44 and Studio integration complete. The pinned
Studio-exported egg passed the installed hardware behavior and reboot checks
below. This is a bounded-subset result, not full V1/V2 feature parity.

The capability contract below records API 44/profile revision 2. API 45/profile
revision 3 supersedes its audio exclusion only with resident sampled SFX; see
[[Peep_Studio_V2_Audio_Export_Handoff]]. Other restrictions remain unchanged.

This extends the existing development-restricted resident export profile, not
the firmware format or the full V2 feature set. No firmware code, wire IDs,
source schema, canonical target-profile constants or GUI files changed.
It supersedes the single-scene/connection restrictions in the API 42 export
handoff and API 43 host-connection handoff for this subset only.

## Capability Contract

Studio must inspect capabilities and project readiness, not enable export
merely because the service API number increased:

- `scene_object_authoring.multi_scene_export=true` in hello.
- Per-scene `multi_scene_export=true`; `export_ready` remains a whole-project
  result. Any bad scene blocks every scene's export-ready flag.
- `package_export.v2_profile.profile_id` remains `hw6_v2_resident_v1`;
  the additive `profile_revision=2` identifies this expanded host admission
  contract. The ID is not a new egg wire version.
- The profile now advertises `scene_connections=true`, `limits.scenes=8`,
  `scene_entry_modes=[fresh_default]`, `scene_exit_action_kinds=[]`,
  `self_scene_exits=false` and `admission_scope=all_scenes_including_unreachable`.
- Existing `project.build_package` is the sole public build operation.
  `build_issues` continues to explain restrictions without blocking draft
  editing, saving or preview.

## Supported Subset

Packages contain one to eight V2 scenes and fit wholly within 65536 bytes,
including the footer. All scenes use continuous interaction and scene-owned
objects. Mixed V1/V2 projects, audio chunks/cues/actions, authored shell-exit
actions, shipping builds and self-scene exits remain excluded.

Cross-scene input routes and scene-timer handlers may have guards but their
action lists must be empty. They enter the destination's default entry state
fresh. No outgoing object, variable, timer or audio mutations are admitted.
Named scene exits remain authoring references, not executable duplicate nodes.
Scene memory, retained scene resume, named destination entries, parallel regions,
runtime text, runtime playback controls and calendar time are not introduced.

Within a scene, the existing supported object, variable and timer operations
are retained. This increment does not broaden global timer availability labels
or imply that every timer/input combination has been hardware-qualified.
Timer evidence remains in [[V2_Timer_Controls_Installed_Test]] and earlier
installed HOME/AWAY results in [[V2_Multiscene_Installed_Test_Runbook]].

## Budget Responses

The profile's `limit_scopes` distinguishes package, scene, clip and display
limits. Package bytes, scene count and strings are package-wide. Graph and
animation budgets apply separately to each scene, including unreachable ones.
Do not sum scene animation cycles or derive a common tick across scenes.

Compatibility reports retain `budgets.waiting_visual_sequences.analysis` for
single-scene clients. For multi-scene projects it is null; use the additive
`scene_analyses` map keyed by source scene ID. The report declares
`scope=per_scene` and retains the conservative budget status and exact-device
admission requirement. Single-scene reports also supply this map.

Each scene retains the existing conservative treatment of hidden clips,
possible horizontal positions, common frame intervals and complete cycles.
Limits remain 12 combined steps, 18 chunks and 10512 payload bytes per scene.
No rounding, relaxed payload limits or reliance on pixel coincidences is added.
Firmware still checks exact private render/payload admission before installation
and runtime transactions; a host-ready project is not proof of panel drawing.

## Pinned GUI Fixture

Source: `examples/authoring/native_v2_lobby_garden_integration.peepproj`.
GUI commit: `79ecdd758c58aff33bac1ee90f6fa4528dcb7091`.
The GUI worktree's fixture was checked against this commit with no differences.
OS read it without changing or saving the GUI project.

API 44 `project.load` reports no build issues and both scenes export-ready.
The ordinary `project.build_package` result is:

- 3944 bytes, container version 2, entry scene `lobby`, two scenes.
- SHA-256: `ce710a24e1c1dbb7e4a48f6c6b55cf0f5d20d00aa37181d1379d0bb24f6ad691`.
- `garden`: 400 ms quantum, four steps, conservative eight chunks / 4672 bytes.
- `lobby`: static HOLD, zero quantum, one step, one chunk / 584 bytes.

Those exact public-service bytes passed native normal install preflight and
installed entry selection (Lobby runtime ID 2, Garden ID 1). Host clock/storage
substitutes are not physical USB, flash, display or current measurements.
For that native check, OS did not generate a replacement embedded C fixture or
change the device's installed package. Studio subsequently produced the same
artifact through its normal public build/export flow, without a development
encoder workaround; see the hardware result below.

## Verification and Hardware Procedure

179 focused tests pass across public export/parser/readiness, service and V1
regressions, V2 objects/creation/graphs/connections, native installed replacement
and timers. Coverage includes eight scenes, ninth-scene rejection, non-first
entry, independent scene timing budgets, bad unreachable scenes, forbidden
exit actions, whole-project readiness and unchanged single-scene egg bytes.
The native installed replacement test now consumes the public compiler output.

The completed integration used the capabilities above and the unchanged pinned
fixture. Repeatable hardware steps:

1. Transfer the Studio egg via USB, install through the shell and select PLAY.
2. Confirm static Lobby, A into Garden, continuous 1-2-3-4 across L/R selection,
   the one-shot two-second TIMER fill, and B back to Lobby.
3. Re-enter Garden and confirm fresh LEFT/frame-1/empty-TIMER entry.
4. Let both scenes idle: Lobby must use HOLD sleep, Garden animated LPBAM sleep.
5. Deliberately reboot and confirm Lobby is the package entry again.

Use `__fw0_object_installed_prints.gdb` for generic source/model/admission,
render and sleep evidence. Its older size-specific fixture text does not
describe this new egg. Reconnecting GDB alone is not reboot evidence.

## Studio Export Hardware Result (2026-09-17)

Backend commit: `007029606a9807d7d46e66e3424f2b827491a3d2` (API 44).
GUI reported normal public `project.build_package` export at:
`G:/PEEPSHOW-PeepStudio/tools/peep-studio/dist/api44-multiscene-export/native_v2_lobby_garden_integration.egg`.
OS independently hashed that file and confirmed the pinned SHA-256 above.

The user installed the Studio artifact through the USB/shell flow and confirmed
the requested visible behavior: Lobby/Garden navigation, numbered animation
continuity across L/R, timer reveal, fresh Garden re-entry and deliberate reboot
back into Lobby. No firmware reflash or development scene-enable helper was
required for this checkpoint.

Captured installed/runtime evidence:

- Installed source 3, execution model 2, size 3944, slot 0, generation 18;
  active scene 1 (Garden), two scenes, activation status 0.
- Three scene replacements, zero failures, last replacement status 0.
- Admission token/completion 20/20, lease 0; profile, schedule and display
  statuses 0. Exact candidate payload: eight chunks / 4672 bytes.
- Display request/completion 15/15, result 0, lease fault 0. These indicate
  completed display work, not merely owner scheduling; visible behavior was
  separately confirmed by the user.
- Garden schedule four steps / 400 ms, publication status 0, LPBAM fault 0.
  WFI returns/measured/reconciled 7/7/7 with clock status 0; automatic STOP2
  entry count 13. Current backend status 1 and blockers 0x1080 were captured
  after wake and are not a completed sleep-admission success snapshot.
- Timer due/dispatch/applied 3/3/3, ignored/error 0/0, RTC timer selections 4;
  the timer was inactive and unpaused at capture.

Counters are cumulative, not isolated before/after deltas for every action.
The supplied capture is Garden, not a separate Lobby HOLD-backend capture.
It supports successful sleep returns but does not measure low-current
residency. No new current measurement or broad timer capability promotion is
claimed. Static HOLD and quiet battery wake have separate earlier validation.

Runtime text, persisted project settings, object names, asset tags, expanded
timer nodes and V2 audio remain separate requested increments, not prerequisites
or implied capabilities of API 44.
