# V2 Scene-Exit SFX Export Handoff

Service API 50, profile `hw6_v2_resident_v1` revision 5. Firmware prerequisite:
`10bd293` (installed V2 scene-exit SFX) or its changes. The bench-tested unit
already has that firmware; this backend increment needs no further reflash.

## Discovery and Authoring

- `service.hello.package_export.v2_profile.scene_exit_action_kinds = [play_sfx]`.
- The same list appears in `service.hello.scene_object_authoring` and each
  `scene_capabilities[scene_id]` record.
- `audio_profile.action_contexts` includes `scene_exit` and `timer_scene_exit`,
  alongside `local_transition` and `local_timer_handler`.
- Audio descriptors advertise `scene_exit_commit = after_destination_admission_and_commit`,
  `scene_exit_rejection = no_cues`, `action_order = authored_order`,
  `sequential_playback = false`, and
  `audio_failure_after_commit = report_without_scene_rollback_or_retry`.
- Use existing route/handler action editing and `project.build_package`.
  Named scene exits remain shared destination links; attaching a cue to a route
  or handler does not invent a trigger or attach actions to a visual alias.
- Continue requiring whole-project `export_ready` and no build issues.

No source schema, wire format, firmware, clock, voice-limit or resident-capacity
changes belong to this backend increment. Package capacity remains 65,536 bytes
including assets and footer. Existing graph/action bounds still apply.

## Semantics

Only `play_sfx` is supported on exits to another scene's fresh default entry.
Empty action lists remain valid. Self exits and outgoing object, variable, timer
or shell-exit mutations remain rejected, including mixed SFX/mutation lists.

Guards and destination admission precede scene commit. Only committed exits
publish cues, once in authored order. Rejected exits leave the source unchanged
and play nothing. A subsequent audio queue/mixer failure is reported without
rolling back the scene or retrying sound. Multiple cues may overlap; action order
is dispatch order, not sequential playback or a wait-for-audio feature.

Existing same-package audio continues across scene changes. Shell/package
suspension stops and discards package SFX; Resume never replays them.
Scene memory, parallel regions and outgoing local mutations remain out of scope.

## Evidence and Next Studio Fixture

The normal public service build reproduces the HW6-tested OS fixture exactly:
54,660 bytes, SHA-256
`610119f8944e1965cb4b29e081085eb957469b33db6cc7c0c8fe71849cac0f5c`.
HW6 evidence covers one cue per input exit, ongoing audio across replacement,
shell stop/discard and successful drain. Native tests additionally exercise
timer exits, two ordered cues, rejected destination admission and repeated round
trips. These do not claim physical multi-cue/timer-exit testing or measured energy.

Create a separate Lobby/Garden exit-audio fixture, preserving the earlier fixture:

1. A leaves Lobby for Garden with one long, recognisable exit cue.
2. B returns to Lobby with one short, recognisable cue; the long cue continues
   if still active.
3. Already-inapplicable A/B inputs do nothing and produce no cue.
4. Keep animation and local selection behaviour, but make local and timer audio
   silent for this fixture so exit cues are unambiguous.
5. HOLD START discards active sound; Resume stays silent.

Build through the normal public command, not a development encoder. Return the
source commit, project/artifact paths, size, SHA-256 and host observations for
installed hardware closure.
