# V2 Resident SFX Export Handoff

Status: 2026-09-17, service API 45 implements public resident V2 sampled-SFX
export. This follows the installed HOME/AWAY hardware result in
[[V2_Installed_SFX_Test]]. A Studio-generated audio fixture remains to be tested.
No firmware, wire format, source schema or canonical target-profile changes are
part of this backend increment.

## Capability Discovery

Use capabilities and whole-project readiness, not the API number alone:

- `service.hello.package_export.v2_profile.profile_id` remains
  `hw6_v2_resident_v1`; `profile_revision` is now 3.
- `package_export.v2_profile.audio=true` and `audio_profile` describe the subset.
- The same descriptor appears at `service.hello.scene_object_authoring.audio_export`
  and `scene_capabilities[scene_id].audio_export` for V2 scenes. Legacy scenes
  return null there and continue using the existing `state_scene_audio` contract.
- The descriptor has `supported=true`, `capability=audio.sampled_sfx`,
  `action_kinds=[play_sfx]` and
  `action_contexts=[local_transition, local_timer_handler]`.
- `export_ready` is still a whole-project result. Capability support does not
  override build issues, even if a particular scene contains no sound.

Existing `audio_asset.upsert/delete`, `audio_cue.upsert/delete`, route/handler
action commands and `project.audio_audition` remain the authoring path. No new
audio mutation command, timer model or GUI compiler is needed.

## Bounds and Lifetime

- One to eight resident V2 scenes; the complete package, including audio,
  visuals, metadata and footer, must fit in 65536 bytes. Audio has no separate
  64 KiB allocation and cannot use the legacy 4 MiB bank allowance.
- An audio catalog has 1..32 sampled assets and 1..64 cues. Assets without any
  cues are an editable draft but cannot be exported in this firmware subset.
- Compiled sound remains mono 16 kHz, 4-bit IMA ADPCM in 256-sample blocks.
  The existing PCM-WAV import path performs conversion. Per-cue priority and
  volume are supported with the existing 0..255 bounds.
- The existing five-voice runtime limit is reported from the target profile.
  This increment does not claim new five-voice concurrent-display stress or
  higher-priority-preemption hardware coverage.
- Local transitions and local timer handlers may contain ordered `play_sfx`
  actions. Targetless handlers do not re-enter the local state. Effects become
  dispatchable only after successful runtime admission.
- Local state changes and same-package scene replacement preserve active SFX.
  Cross-scene routes/handlers still require EMPTY action lists: continuity of
  existing sound does not grant sound actions on scene exits.
- `package_suspend=stop_and_discard`, `package_resume=new_requests_only`, and
  `package_exit_or_replacement=stop_and_discard`. A long clip is not resumable
  music. HOLD START opens the shell; returning never replays discarded SFX.
- Music, looping, procedural audio, pause/resume, fades, runtime volume controls,
  nonresident V2 audio and scene-exit actions remain unsupported. Shipping,
  mixed execution models, scene memory and parallel logic remain excluded.

The required firmware contains commit
`b4784e2efbd6925719dae84ebb0ed2bf847e9453` or its installed-SFX admission changes.
Older V2 firmware rejects audio during install preflight. The unit used for the
recorded SFX test already has the required firmware; this backend increment
does not require another reflash on that unit.

## Reports and Validation

The public compiler and parser enforce the same resident profile. They retain
integrity, ADPCM, cue-reference, volume/priority and action checks; audio does
not bypass per-scene graph or LPBAM limits. Oversized banks, extra assets/cues,
malformed catalogs and prohibited exit actions block export, not draft editing
or audition.

`budgets.audio` reports compressed asset bytes and a 65536-byte upper bound,
with `residency=whole_package`, `shared_with=package_size` and
`independent_budget=false`. Use `budgets.package_size` and `build_issues` for
actual total package fit; other content and padding consume the same capacity.
Timer-only SFX requests appear in scene required-capability and package capability
reports. The general development target's shipping-pending advisory and global
timer availability labels are unchanged; they are not new V2 audio restrictions.

The normal public build of the OS hardware-tested source bundle is byte-identical:

- 54696 bytes, two audio assets/two cues.
- SHA-256: `af1fe2d09a3b527b47a640a355be318f29a37380315c9ee5e7ad53cea5feff9c`.
- 50920 compressed asset bytes; firmware reports 50924 bank bytes including
  alignment padding. Neither figure includes all package overhead.

The installed native test now uses public `build_egg` output. Coverage includes
normal service build/readiness, capability agreement, exact bytes, timer-only
sound reports, audition, 32/64 catalog boundaries, invalid cue metadata,
65536-byte acceptance/oversize rejection, forbidden exits and installed
runtime admission/effect/catalog-lifetime tests. Native checks do not play sound.

Verification: 194 focused service/export/authoring and native replacement/timer/
audio tests passed. `gen_target_profile.py --check` passed; firmware-generated
limits are unchanged. No firmware rebuild is required by this backend change.

## GUI Next Fixture

Keep the passed Lobby/Garden integration fixture unchanged. Create a separate
`native_v2_lobby_garden_audio.peepproj` using the existing authoring commands:

1. Preserve static Lobby, Garden's numbered 400 ms animation, L/R marker and
   fresh-entry two-second TIMER reveal.
2. Add a short audible SFX to each matched Garden L/R selection change.
3. Add an approximately six-second audible SFX to the targetless TIMER handler.
4. Keep A/B scene-exit actions empty. B during the long tone should enter Lobby
   while the tone continues. HOLD START during playback must stop it; Resume
   must be silent, with fresh local cues working afterward.

Use normal `project.build_package` once whole-project `export_ready` is true.
Return the source path/commit, exported egg path, size and SHA-256. Do not use
the OS development encoder or silently shorten/drop assets to fit. Host-check
the authored behavior first; OS will verify USB install, audible behavior,
sleep/wake and deliberate reboot with the Studio artifact.
