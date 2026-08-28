# Peep Studio and PeepOS Link Contract

Status: `active_handoff`

Implementation status: `Stage_3_STATE_presentation_backend_ready_GUI_pending`

This document is the working boundary between Peep Studio development and
PeepOS/HW6 bring-up. It tells an editor agent what the platform actually
supports today, which host interface to use, and which planned features must
not be presented as implemented.

Related:

- [[Authoring_Tool_Architecture]]
- [[Authoring_Project_Schema_Contract]]
- [[Peep_Studio_UX_Direction]]
- [[HW6_Authoring_Vertical_Slice]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Display_and_Rendering_Contract]]
- [[Power_and_Sleep_Policy]]

---

## Authority

Peep Studio is a client of the Python authoring service. The React application
must not independently interpret project semantics, emulate package limits, or
compile `.egg` bytes.

Authority order:

1. platform and package contracts under `docs/`
2. `tools/authoring/peepshow_authoring/`
3. versioned authoring-service responses
4. Electron desktop integration
5. React presentation state

If the UI disagrees with a service validation result or compatibility report,
the service result wins.

---

## Current Capability

The following path is implemented. Rows explicitly marked pending still require
their stated HW6 proof; the remaining rows have been exercised on target.

| Area | Current capability |
|---|---|
| editable source | directory-based `.peepproj` project with JSON scene and asset sources |
| package output | deterministic `.egg` binary with SHA-256 integrity |
| scene type | STATE |
| state execution | bounded variables, input routes, guards, actions, and deterministic transitions |
| package scene flow | direct STATE-to-STATE replacement is implemented and proven on HW6 through service API 8, PKG1 graph V2, and FW0 runtime API 11 |
| input | logical A, B, L, R, short START, JOY_LEFT, JOY_RIGHT, JOY_UP, and JOY_DOWN sources |
| visuals | package-backed native-scale masked 1bpp sprite frames |
| retained render model | bounded ordered scene elements with binary alpha and four platform planes |
| package primitives | retained line, outline rectangle, filled rectangle, circle, and ellipse records are compiled, previewed, loaded, and target-proven; private shell/calibration draw helpers remain unavailable |
| package text | service API 15 rasterizes printable-ASCII menu labels through `peepshow.system.8x8.basic.v1` into ordinary masked 1bpp sprite frames; runtime text remains unavailable |
| package audio | not exposed; HW6 currently proves only a generated diagnostic speaker tone, not `.egg` audio assets or STATE SFX actions |
| STATE animated elements | bounded repeating sprite phase timelines with 1..4 frames, 1..12 combined steps, explicit cadence, and a settled step; mixed 2-phase and 3-phase composition and deterministic fallback are target-proven |
| awake preview | exact 168x144 package-backed framebuffer with deterministic fake time and side-effect-free scene thumbnails |
| STOP2 | package visuals compiled into LPBAM animation and resumed across wake/STOP2 handoff |
| firmware package proof | embedded and USB-installed `.egg` packages load, validate, resolve STATE content, handle input, render package pixels, replace STATE scenes directly, animate in STOP2, and return to shell; installed packages currently run through a `65536`-byte RAM cache; CONTINUOUS/TIMEOUT interaction lifecycle and manual inactivity are target-proven |

Measured hardware behavior, current SRAM4 admission limits, and power figures
remain hardware evidence. The desktop preview must not claim to reproduce
current draw or prove STOP2 behavior.

---

## Joystick Integration Boundary

Peep Studio may expose `JOY_LEFT`, `JOY_RIGHT`, `JOY_UP`, and `JOY_DOWN` as
STATE input sources for the HW6 FW0 target profile. The authoring validator,
compiler, package parser, headless preview, FW0 package loader, and runtime
input router use the same stable logical IDs `6..9`.

These sources are edge-like logical activations produced after Platform-owned
calibration, normalization, hysteresis, and direction resolution. The editor
must not expose raw TMAG readings, calibration records, magnetic thresholds,
wake-and-sleep configuration, STOP2 wake pins, or power policy as package
controls. Desktop preview injects the selected logical source directly; it does
not simulate sensor current, wake latency, or physical threshold behavior.

HW6 has target-proven persistent guided calibration, awake canonical diagonal
detection, deterministic dominant-axis four-way resolution, and movement wake
for positive and negative X/Y directions. Movement wake adds approximately
`10 uA` in the matched five-minute STOP2 comparison (`55 uA` disabled versus
`65 uA` enabled). This is Platform evidence, not package-authored behavior.

The current STATE authoring subset exposes four cardinal joystick actions only.
It does not yet expose eight-way diagonal action IDs, normalized vector
bindings, per-scene four-way/eight-way selection, long-press/repeat policy, or
PROGRAM-scene vector polling. Those controls must remain unavailable until the
Engine contract, target profile, package schema, preview, and firmware runtime
all implement the same semantics.

---

## Interaction Lifecycle Boundary

Peep Studio must expose one package-level interaction mode:

- `CONTINUOUS` disables automatic inactivity timeout while retaining the system-owned manual-INACTIVE gesture;
- `TIMEOUT` uses the PeepOS-owned inactivity interval and requires each current
  STATE scene to declare `preserve` or `exit_shell` as its inactive route.

Authors select the mode, inactive route, and which declared input routes count
as meaningful activity. Peep Studio must not expose the numeric timeout, RTC
wake timer, `PRESS START` cue duration, activation-animation timing, joystick
wake threshold, or wake-pin configuration. Those remain target-owned Platform
policy.

For the HW6 FW0 target, `TIMEOUT` suppresses package input and joystick movement
wake after expiry. `START` is consumed by PeepOS to restore `ACTIVE` state
whether or not the `PRESS START` cue is visible, then HW6 shows one bounded
system-owned eye-opening animation before revealing the active package
presentation. A/B/L/R remain consumed while inactive and request one bounded
system `PRESS START` cue before the prior waiting presentation and STOP2 resume.

While `ACTIVE`, Peep Studio may bind a short `START` press like any other
declared package input. Firmware emits that action only when START is released
before the target-owned 2-second manual-INACTIVE threshold. Reaching the
threshold consumes the gesture for PeepOS, enters `INACTIVE` in either package
mode, and does not emit the package action. Continued hold remains the
system-owned shipping gesture. `CONTINUOUS` manual inactivity always preserves
the current scene, so it does not require an authored inactive route.

The compiler emits this policy in PKG1 `STG1` format version 3. Peep Studio may
edit the normalized source fields now; it must not invent desktop-only timeout
or wake behavior that the package cannot encode.

---

## STATE Presentation And Asset Boundary

Peep Studio does not call firmware drawing functions. It authors retained
elements and source assets, the Python service compiles them into portable
`.egg` records, and PeepOS validates and composes those records into the native
168x144 1bpp framebuffer. Immediate commands such as `draw_circle()` or direct
framebuffer access are not part of the authoring or package API.

The scene canvas owns integer panel-native bounds, package-visible layer,
visibility, ordering, asset/frame references, and animation bindings. Package
content may target `BACKGROUND`, `SCENE`, or `UI`; `OVERLAY` remains
Platform/Engine-owned. Host preview must consume the same compiled records as
firmware rather than reproducing them with a separate React renderer.

The current generic executable presentation record is `RND2`. It supports
native-scale masked 1bpp sprite frames plus bounded `line`, `outline_rect`,
`filled_rect`, `circle`, and `ellipse` primitives. Records carry package layer,
visibility, z-order, and integer panel-native bounds. The compiler, package
parser, exact host preview, HW6 loader, retained model, and display owner now
implement those same semantics. On-target visual, scene-replacement, and STOP2
proof passed on 2026-08-27. New packages emit `RND2`, while `RND1` remains
load-compatible.

The repository also contains private firmware drawing helpers for calibration,
activation, and shell UI. Their existence does not expose those helpers to
authored packages. Authored text is compiled into ordinary masked 1bpp sprite
assets and does not call those private helpers.

STATE animation terminology is strict:

- a retained STATE sprite selects one compiled frame for its settled/base presentation;
- a STATE animated element adds a bounded repeating `waiting_visual` phase map
  to that sprite, and this is the only package animation that may run while a
  reactive scene waits or the MCU is in STOP2;
- a general `frame_animation` may have arbitrary authored length and timing,
  but it is not STATE-placeable and is reserved for SEQUENCE authoring;
- static primitives remain valid STATE elements, but their geometry is not animated.

The STATE-first presentation expansion is:

1. **Hardware-validated:** serialize package layer, visibility,
   order, bounds, and sprite/frame references without special proof names;
2. **Hardware-validated:** expose bounded retained line, outline
   rectangle, filled rectangle, circle, and ellipse records with deterministic
   integer rasterization;
3. **Toolchain-implemented:** rasterize initial authored menu text into masked
   1bpp package assets at build time through the frozen 8x8 system font;
   runtime fonts and mutable text remain a later capability;
4. **Next:** add bounded STATE actions for show/hide, move, frame selection, and animation
   selection, with dirty-region composition owned by PeepOS;
5. **Next:** add one symbolic bounded STATE SFX action backed by a compiled sampled-audio
   asset and owned at runtime by `thAudio`.

Visual assets, primitive records, text-derived sprite assets, and audio assets
are package data. They must never contain display-driver calls, framebuffer
pointers, SAI/DMA configuration, source paths, or host-only objects.

---

## Not Yet Exposed

The following are planned or incomplete and must be labelled unavailable in
the editor until this document is updated:

- SEQUENCE and PROGRAM scene authoring or execution;
- Peep Studio controls for the backend-ready retained-element, asset-catalog,
  waiting-timeline, and STATE graph mutation commands;
- arbitrary desktop fonts, runtime text, and runtime element mutation actions;
- sampled package audio, STATE SFX actions, audio audition, or audio
  compatibility reporting;
- 4-tone and 16-tone fixed dither asset import;
- maps, tile layers, runtime fonts, rotation, or interpolated scaling;
- package installation or activation on a connected device;
- external storage package discovery and the production eggless shell path;
- a conformance-qualified HW6 digital twin;
- arbitrary game-defined native code or direct hardware access.

---

## Service Boundary

Run the long-lived service from the repository root:

```powershell
python -u tools/authoring/egg_tool.py service
```

Transport is newline-delimited JSON over stdin/stdout. The current transport
protocol is version `1`; the current service API is version `15`.

| Operation | Purpose |
|---|---|
| `service.hello` | discover service/API versions and supported operations |
| `service.shutdown` | request orderly sidecar shutdown |
| `project.load` | load and validate one `.peepproj` directory |
| `project.validate` | return authoritative issues and semantic hash |
| `project.normalize` | return canonical normalized project data |
| `project.build_package` | compile authoritative `.egg` bytes and compatibility report |
| `project.compatibility_report` | inspect target/resource compatibility without exporting |
| `project.apply_commands` | apply typed semantic edit commands and return a new project revision |
| `project.save` | persist the current in-memory project manifest, scene records, and asset catalogs to authored source files |
| `project.undo` | undo the last accepted command within the bounded service history |
| `project.redo` | redo the last undone command within the bounded service history |
| `project.scene_thumbnails` | return one side-effect-free initial framebuffer snapshot per compiled STATE scene |
| `project.preview_reset` | start one selected STATE scene directly |
| `project.preview_input` | inject one logical A/B/L/R, short START, or JOY_LEFT/JOY_RIGHT/JOY_UP/JOY_DOWN input |
| `project.preview_advance` | advance deterministic preview time by an explicit duration |

Every project operation after load uses `project_revision`. Every preview
operation after reset also uses `preview_revision`. Stale revisions must be
surfaced to the user; the UI must not silently retry them against newer state.

The framebuffer returned by preview is `168 x 144`, row-major,
MSB-first 1bpp, where a set bit is black. Peep Studio must render those bytes
without filtering or reinterpretation.

---

## Desktop Ownership

Electron main process owns:

- Python sidecar lifecycle;
- project and export dialogs;
- local filesystem access;
- future device-tool process integration.

The context-isolated preload exposes only named operations. The renderer stays
sandboxed with Node.js integration disabled.

React owns:

- project and scene presentation;
- user intent and transient selection;
- exact framebuffer display;
- diagnostics returned by the service.

React does not own project validation, package compilation, compatibility
policy, preview execution, filesystem access, or hardware policy.

---

## GUI Agent Handoff

The Peep Studio agent may work under `tools/peep-studio/` and may add host-side
tests and editor documentation. It should not modify firmware while performing
editor work.

Before changing an editor feature:

1. check this document for current platform support;
2. use `service.hello` rather than assuming an operation exists;
3. add UI around service results without duplicating their rules;
4. keep direct selected-scene preview available so an author never has to play
   through an entire egg to test one scene;
5. mark unsupported scene types and asset modes explicitly;
6. request a service/API extension when semantic information is missing.

Initial shell location: `tools/peep-studio/`.

The current shell can open a project, select and run a STATE scene, inject
buttons, advance or play deterministic time, inspect runtime state, build, and
export `.egg`. Service API 14 now supplies the semantic mutation boundary for
complete single-scene STATE graph authorship; the GUI branch may add controls
without directly editing normalized JSON in React.

---

## GUI Delivery Order

Peep Studio development follows this order. Later stages must not bypass the
Python mutation, validation, and revision boundary established in stage 2.

### Stage 1: Read-Only Scene Inspection

- show normalized STATE nodes, transitions, variables, render elements, and
  waiting visuals;
- keep selected-scene direct preview available;
- show source IDs and authoritative validation without modifying the project.

Implementation status: connected in Peep Studio. The editor now presents a
read-only per-scene STATE graph from normalized service records, keeps the
direct selected-scene framebuffer preview, and exposes read-only inspectors for
scene variables, routes, guards, ordered actions, render elements, and waiting
visuals. Project mutation controls remain deferred to Stage 2.

### Stage 2: Authoritative Editing Foundation

- add typed Python service commands for semantic project changes;
- return a new `project_revision`, normalized affected records, and diagnostics
  after every accepted command;
- add project save, dirty-state handling, and command-based undo/redo;
- reject stale revisions rather than silently replaying edits.

No scene-canvas or graph control may directly mutate normalized JSON in React.

Implementation status: complete for the authoritative editing foundation.
Service API version 14 exposes `project.apply_commands` for bounded state,
render-model, variable, input-action, route, guard/action-list, wait-policy,
interaction-policy, retained-element, waiting-visual, and masked-1bpp asset
mutation. Generic deletes reject referenced records; callers detach references
explicitly in an ordered command batch before deletion. Batches contain at
most 64 commands, while `service.hello.state_scene_graph` publishes command
families and schema limits for feature detection. Catalog animation edits
persist for future SEQUENCE work but are explicitly not STATE-placeable.
`editor.scene_flow.set_node_position`
owns editor-only scene-flow layout. Local Logic state cards use
`editor.state_graph.set_node_position`, storing per-scene, per-state editor-only
layout coordinates that must not affect package bytes.
Accepted commands update the in-memory Python-owned project, return a new
`project_revision`, normalized document, diagnostics, and dirty state, and
reject stale revisions.
`project.save` persists the current in-memory project manifest, scene records,
and asset catalogs back to their authored JSON files and clears dirty state. Peep Studio also
exposes Save As for copying the current `.peepproj`
directory to a user-chosen location; checked-in examples opened through the
example button are temporary copies. `project.undo` and `project.redo` keep a
bounded 32-step command history in the Python service; new edits clear redo, and
dirty state is computed against the last saved semantic project hash.

Service API version 15 adds deterministic build-time text assets through the
existing `asset.upsert`/`asset.delete` commands. Peep Studio discovers the
exact font ID, glyph cell, character set, scaling bounds, ink/background, and
one-frame output contract through
`service.hello.state_scene_presentation.build_time_text`.

### Stage 3: Scene Canvas And Visual Elements

- add, remove, select, move, and reorder retained visual elements;
- edit bounds, platform layer, focus ownership, sprite/frame reference, and
  bounded STATE waiting-phase binding;
- add an asset browser for the implemented native masked-1bpp PNG subset;
- expose only visual controls supported by the selected target profile and
  compiled package schema; do not map canvas tools to firmware draw calls;
- add editable text elements that are deterministically rasterized to masked
  1bpp package pixels at build time;
- keep spatial placement on the scene canvas rather than the behavior graph.

This is the first stage at which the user can begin laying out real menu
screens. Build-time text rasterization is the initial menu-label path; it does
not require or imply an on-device runtime font renderer.

Implementation status: placement mode shell is present in Peep Studio. It
keeps the fixed project-panel preview available in every mode, promotes the
selected scene preview into the main workspace for placement, shows the current
scene object hierarchy in the project panel, and reserves the inspector for the
selected object's placement/properties. The placement display has a faint
screen-space grid, selectable retained-element overlays, a floating primitive
tool palette, drag movement, shape resize handles, line endpoint handles,
sprite placement from compiled asset frames,
inspector sprite-frame selection,
inspector X/Y fields, layer/visibility controls, forward/back draw-order
buttons, and inspector-level selected-object deletion. These controls call the
Python service commands (`render_element.add`, `render_element.delete`,
`render_element.set_position`, `render_element.set_bounds`,
`render_element.set_layer`, `render_element.set_visibility`,
`render_element.set_z_order`, and `render_element.set_visual_ref`) rather than
directly editing normalized JSON in React. The Assets workspace now lists
compiled sprite assets, shows frame previews, and can import a PNG as one
full-image masked 1bpp sprite frame through Electron filesystem ownership and
the Python `asset.upsert` command. Imported PNGs are sanitized before catalog
creation: fully transparent pixels remain transparent, and visible pixels are
thresholded to pure black or pure white. Sprite assets and frames may also carry
optional author-facing `display_name` labels; stable asset/frame IDs remain the
service, package, and firmware references. Sprite-sheet slicing and audio
controls remain future Assets workspace work. Circle and ellipse editing preserves
the current RND2 constraints: odd-sized bounds of at least 3 pixels, with
circles remaining square. Filled circle and filled ellipse controls remain
deferred until the package/service and firmware expose those primitive
semantics. Grid visibility, grid strength, major grid lines, overlay boxes, and
label display are editor view settings in the project panel only; they do not
affect package output. React controls for build-time text labels and the phase
timeline are the next GUI-branch work. Text controls author a
`system_font_text` asset and place its compiled frame as a normal sprite; they
must not imply runtime text editing. The GUI must not offer general
`frame_animation` binding in STATE; it edits `waiting_visual` phases instead.

### Stage 4: STATE Graph Editing

- create, remove, rename, and select STATE nodes;
- edit the one declared entry state;
- present local logic as state/prefab node cards with trigger output rows, not
  as a UML-first edge editor;
- create and remove deterministic state-transition outputs and their single
  destination edges through Python service commands;
- edit logical A/B/L/R and JOY_LEFT/JOY_RIGHT/JOY_UP/JOY_DOWN routes, guard
  expressions, ordered actions, and target state through typed inspectors;
- support prefab-backed menu nodes through declared slots and read-only generated
  internals before exposing arbitrary custom internals;
- validate unresolved targets and bounded-capacity limits before save/export.

At the end of this stage, an author can build a complete interactive menu that
fits within one STATE scene.

Implementation status: local logic presentation has been reshaped toward the
user-facing vocabulary. State graph cards show screen names, entry/output
badges, and trigger output rows. The primary inspector uses transition,
condition, and effect language while keeping Python-owned routes, guards, and
actions as the underlying semantics. The Python backend is complete for one
STATE scene: it can create/delete states and render models, choose the entry
state, create/update/delete variables and logical inputs, create/delete and
retarget routes, edit route sources and input bindings, and add/delete/reorder
guards and actions. Reactive-wait and interaction policies are also replaceable
through typed commands. The next GUI work is to expose these API 14 commands
through the existing node-card and inspector model.

### Stage 5: Package Scene Flow

- create and remove package scenes;
- provide a package-level flow view whose nodes are scenes, not states;
- author declared scene-transition routes, entry behavior, return behavior,
  `transition_scene`, and `exit_to_shell` actions;
- validate every scene target and package entry scene;
- gate route export against the selected target profile. HW6 FW0 may export the
  proven direct actionless STATE-to-STATE subset; push/pop, return stacks,
  cross-scene actions, and transitions to other scene types remain disabled.

At the end of this stage, an author can build a multi-screen menu hierarchy.
Scene-flow editing must remain separate from the STATE graph inside each scene.

Platform foundation status: implemented and proven on HW6. Authoring schema
routes now accept exactly one `target_state` or `target_scene`; the direct
scene-transition capability introduced by service API 8 is retained in service
API 12; selected-scene preview follows a scene target; and the compiler emits
PKG1 `STG1` version 3. Direct replacement enters the destination entry state,
resets destination-local variables, and starts a new timeline epoch. Peep
Studio may expose these exact semantics for the HW6 FW0 target profile. It must
not expose cross-scene route actions, push/pop, return stacks, or
STATE-to-SEQUENCE/PROGRAM transitions yet.

Peep Studio implementation status: first package scene-flow view is
implemented. It presents scenes as separate package-level nodes and shows
existing `target_scene` route edges from normalized service data. Scene cards
show their existing scene-exit outputs as selectable rows and render real
Python-generated initial scene thumbnails through `project.scene_thumbnails`.
Existing actionless `target_scene` routes can be retargeted to another STATE
scene through the inspector or by dragging an existing exit row to another
scene's entry row. New scene-flow exits can be added from scene cards through
an empty output slot that prompts for the trigger before calling
`route.add_scene_exit`, and deleted through `route.delete_scene_exit`.
Scene-flow cards can be manually rearranged; saved
positions live under project editor-only layout metadata and must not affect
compiled package bytes.

### Stage 6: Animation Timeline

- add and remove authored phase visuals;
- edit phase selection and the fixed 250 ms authoring quantum;
- preview combined mixed-phase timelines;
- show both preferred target admission and the deterministic three-step HW6
  fallback mapping;
- expose package/resource diagnostics returned by the Python service.

### Stage 7: Expanded Asset And Scene Support

- add the bounded STATE sampled-SFX path: WAV import, deterministic conversion,
  symbolic cue binding, host audition, package validation, and target proof;
- add retained line, rectangle, circle, and ellipse elements plus bounded
  element visibility, position, frame, and animation actions;
- add formal font and localization package records when required;
- add fixed 4-tone and 16-tone dither profiles;
- add maps, tile layers, scaling, transforms, and richer retained rendering;

### Stage 8: Later Scene Types

- add SEQUENCE authoring, then PROGRAM authoring, only after the STATE visual,
  input, waiting, scene-flow, and bounded-SFX surfaces are coherent across
  Studio, compiler, package, preview, and HW6 runtime.

### Menu Authoring Availability

| Delivery point | What the user can author |
|---|---|
| current shell | inspect, run, validate, build, and export an existing STATE project |
| after stage 3 | visually compose static menu screens with sprites and build-time text |
| after stage 4 | create functional interactive menus within one STATE scene |
| after stage 5 | create complete multi-scene menu and navigation hierarchies |
| after stage 6 | visually author and budget menu waiting animations |
| after stage 7 | author bounded geometry, element updates, and sampled STATE SFX |

The firmware shell remains system-owned. Peep Studio may author the same source
model as a test `.egg`; accepted system-menu sources may later be compiled into
firmware without making the installed shell package-editable at runtime.

---

## Editor Surface Separation

Peep Studio uses distinct views for distinct semantics:

- the **scene canvas** owns sprites, build-time text, bounds, layer order,
  focus visuals, and spatial composition;
- the per-scene **STATE graph** owns the entry marker, state/prefab node cards,
  trigger output rows, and deterministic state-transition destinations;
- the package **scene-flow graph** owns scene nodes and declared cross-scene
  routes;
- inspectors own route, guard, action, target, and typed property details;
- the STATE waiting timeline owns bounded repeating phase visuals, combined
  steps, cadence, and settled step; the future SEQUENCE timeline owns
  arbitrary-duration animation.

STATE nodes and scene nodes are not interchangeable. Visual elements must not
be represented as behavior nodes merely because both surfaces support
selection and connection UI.

---

## Keeping This Link Current

When PeepOS or the authoring backend adds a capability used by Peep Studio, the
hardware/backend agent updates:

1. the governing platform or package contract;
2. the service API and its protocol tests when a new operation or field is
   required;
3. the Current Proven Capability or Not Yet Exposed section above;
4. the service/API version when compatibility rules require it.

When Peep Studio needs information not currently exposed, the GUI agent records
the concrete use case and requested typed field here instead of reading
firmware sources or reproducing firmware constants in TypeScript.

This file is the synchronization point. It is not a replacement for the
domain contracts and must not invent platform behavior.
