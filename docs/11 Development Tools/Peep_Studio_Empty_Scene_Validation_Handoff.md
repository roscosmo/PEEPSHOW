# Peep Studio Empty Scene Validation Handoff

Date: 2026-09-09.

Status: shared validation and Studio diagnostics implemented; target retest
pending. The original handoff was documentation-only. The GUI follow-through
adds build-readiness checks without changing firmware or user projects.

## Implementation Follow-Through

- Service API `38` includes `build_issues` alongside draft source `issues` on
  project document responses. Each empty-state issue includes scene/state IDs
  and the resolved render-model reference. Studio lists these in the project
  inspector with a locate-scene control.
- Shared build, CLI export and embedding reject incomplete presentations;
  strict compiled parsing/inspection also rejects zero-element render models.
  Existing output files are not overwritten on readiness failure.
- Host preview, thumbnails and audition use an explicit internal draft path.
  Draft validity and save/reopen are unchanged; no empty package is returned
  by the export service. Compatibility reports block package output without
  blocking draft preview.
- Studio clears previous build results before attempting a new build and
  disables export while busy. No source-validation success is described as
  proof of package readiness.
- Regression coverage includes draft creation/save/reopen, stable multi-scene
  diagnostics, primitive-only builds, old empty binaries, output preservation,
  and the production native runtime's zero-element rejection.

This does not repair already exported or installed eggs. The target acceptance
steps below remain required; host checks are not hardware proof.
OS candidate preflight and boot/PLAY error recovery are also implemented with
local native regression coverage. These complement the GUI/shared readiness
checks above; hardware retest remains pending.

Related:

- [[Rendering_API_Contract]]
- [[Peep_Studio_PeepOS_Link_Contract]]
- [[Peep_Studio_Scoped_Timer_Handoff]]
- [[Shell_and_UI_Navigation_State_Machine]]
- [[Package_Workflow_Validation_Runbook]]

## Agreed Rule

Scenes must not be empty. For the current HW6 STATE pipeline, every state in
every exported scene must resolve to at least one valid render element.
Enforce this in shared build/export validation, not only in GUI controls.

"Empty" means no visual objects in the resolved render model. It does not
mean no exit, input binding, focus object, animation, sound or sprite asset.
A supported static primitive alone can satisfy the visual minimum.

Empty scenes may exist as incomplete editor drafts and be saved. They must
not produce an installable egg until the author supplies visual content.
Creating a scene must not become impossible because it starts empty; separate
draft editing validity from blocking build readiness where necessary.

Do not relax the native loader to accept zero-element scenes. The earlier
investigation proposal to do that was superseded by this user decision. Do
not automatically delete empty scenes, wire exits, or insert dummy graphics.

## Reproduced Package

The inspected export is:

```text
G:\PEEPSHOW-PeepStudio\workbench\peep-studio\Authoring_pass.peepproj\authoring_pass_test.egg
package ID: dev.peepshow.authoring_pass
version: 0.1.0
target: hw6_fw0_development
entry scene: main
size: 33120 bytes
SHA-256: e8c5445a4e9c3aba20b239ddb087dc984394f0a1dc167dca58379ff492387c2e
```

The authoring tool on OS commit `d99a9ec7b0c66ada9b014b329fdbfba2067d8c02`
accepted this exact file through `egg_tool.py inspect`. That is evidence of
the shared/native validation mismatch, not proof that this egg is safe to
launch. The SHA-256 above fingerprints the inspected file; it was not read
back as a full-file digest from the device.

| Scene | Compiled scene ID | Presentation | Navigation |
|---|---|---|---|
| `credits` | 1 | `start` resolves to `scene_placement.start`, zero elements | No routes |
| `game` | 2 | One sprite element | B press returns to `main` |
| `main` | 3 | Three states, each with three elements | Eight routes |
| `settings` | 4 | `start` resolves to `scene_placement.start`, zero elements | A press returns to `main` |

Both `credits` and `settings` violate the visual minimum. Adding only an
exit to `credits` would fix neither violation. The installed main scene is
populated; its own emptiness is not the reason launch failed.

## Runtime Evidence

The attached debugger session reported:

- RTOS/owner/package-source APIs `82/85/4`; loader API `18`.
- Selected installed generation `15`, address `0xc0000`, size `33120`.
- Storage read `33120` bytes successfully; resident capacity `65536`.
- Hash status `0`; loader status `1`, reason `11` (`RENDER`).
- Loader entry scene `3`, selected scene `1`, scene decode count `2`.
- Scene active `0`, activation status `1`; runtime lifecycle `5` (`ERROR`).
- UI remained page `0` (`BOOT`), with no UI transitions and no scene rendered.

The original failure was explained by these code paths (before the OS fix):

| Location | Relevant behavior |
|---|---|
| `Core/Src/ps_egg_state_loader.c`, `PS_EggBuildBinding` | Rejects `element_count == 0` and reports `PS_EGG_STATE_LOADER_REASON_RENDER` through scene decoding |
| `Core/Src/ps_scene_runtime.c`, `PS_SceneRuntime_EnterStateScene` | Validates other included scenes before activating the entry scene; scene 1 is `credits` |
| `Core/Src/ps_scene_runtime.c`, visual-binding validation | Also rejects zero render elements |
| `Core/Src/display_renderer.c`, `DisplayRenderer_ValidateSceneModel` | Also rejects zero render elements |
| `Core/Src/ps_hw6_rtos_probe.c`, `PS_HW6_RTOS_RequestPackageInstallErrorUi` | Only requests an error UI while package state is `INSTALLING`; boot failure is not reported through this helper |

All code paths above are under `firmware/peepshow_hw6_fw0/`. The blank BOOT
screen is a separate recovery bug: it is not a valid empty scene being drawn.
The original report also described remaining on the installed PLAY prompt;
the preserved debugger evidence here is from the subsequent boot failure.

## GUI And Shared Tooling Work

1. Add a shared blocking readiness check for every state-resolved render model
   in every included STATE scene. Apply it after resolving placement and
   state overrides, so checking only the authored object list is insufficient.
2. Return structured diagnostics with scene and state identifiers and the
   relevant visual-model reference. Report all offending states in stable
   order, so the author can correct `credits` and `settings` together.
3. Present the diagnostic in Peep Studio and let the author locate the scene.
   Example: `Scene "credits", state "start": place at least one visual object
   before building.` Use existing diagnostic conventions; this document does
   not introduce a new service method or diagnostic-code ABI.
4. Enforce the same rule for GUI builds/exports, CLI builds and embedded-egg
   generation. Validate compiled eggs too; existing bad exports must not keep
   passing `inspect` merely because their container and checksums are valid.
5. Check before overwriting an existing successful export, and do not present
   a previous egg as the result of a failed current build. Preserve source
   drafts and any unrelated GUI changes.
6. Add shared and native parity regressions. Keep firmware's minimum intact;
   coordinate any fixture/test harness changes with the OS branch.

No change to the egg binary format, timer semantics, sensor capabilities or
exit-node requirements is requested. The scoped-timer GUI work can continue.

## Acceptance Cases

- A populated `main` plus empty `credits` and `settings` fails export with
  diagnostics for both empty scenes, even when they are not selected in the
  editor. A route out of `settings` does not suppress its error.
- An exported empty scene fails even if unreachable from the entry scene.
- One state resolving to zero elements fails even if other states in the
  same scene contain visuals. Placement inheritance/overrides are resolved
  before counting render elements.
- An empty scene with input routes, timers or audio still fails. An asset in
  the catalog without a placed render object does not satisfy the rule.
- A static scene with one supported primitive, no sprite assets, no focus
  and no animation passes this visual-minimum check.
- A populated scene without an exit passes this visual-minimum check; any
  navigation diagnostic is separate and must not call it visually empty.
- The shared compiled-package validator rejects this known bad egg. Native
  tests reject the equivalent zero-element scene for the same reason.
- Creating, editing, saving and reopening an incomplete draft still works;
  build/export remains blocked until it is completed.
- GUI, CLI and embed failure do not replace a previous good export or report
  it as a new successful build.

## Confirming The GUI Eggs

After the validation change, revalidate the current GUI projects. The author
must add intended visual content to each empty state presentation or
explicitly remove unwanted scenes and repair their references. Do not alter
the user's test project automatically.

Build a fresh egg and record its fingerprint; require both shared validation
and native runtime validation to pass for all scenes. Then install that exact
file on hardware and confirm PLAY launches the authored `main`, secondary
scenes render, and a subsequent normal boot launches successfully. A valid
container or a completed flash install alone does not confirm runtime loading.

OS native regressions now reject an empty render model in each included scene
and state, including this exact artifact when its recorded fingerprint matches.
They also check rejection leaves the live loader and scene unchanged. Hardware
confirmation remains pending. Updating either validator does not repair the egg
already installed on the device.

## OS Implementation Status

Firmware must still handle an incompatible or damaged package with a usable
shell/error page after either PLAY or boot. An error must not leave the UI at
BOOT or the old installed prompt. Export checks reduce invalid inputs but do
not remove the need for runtime validation and failure recovery.

The OS branch now runs complete native preflight before `VALID` and again before
flash writes, using isolated loader scratch and the runtime owner's HASH path.
Every scene/state is checked, not just the entry scene. Boot and PLAY loader
failures now restore shell runtime ownership and retain a recoverable package
error request for the UI owner to consume. B reaches package tools and START
reaches the shell menu without retrying the bad egg. Visible shell pages own
buttons and joystick input even with no runtime or a failed package; the locked
fatal shell-error screen is not used for package failures. Local tests now
exercise actual UI-router navigation, not just error-event delivery. These
changes have a successful Debug build, not a hardware pass yet.

The GUI agent should still implement the readiness diagnostics above. Firmware
rejection is a second line of defense, not a substitute for actionable authoring
errors. No Peep Studio implementation files were changed by the OS workflow fix.
