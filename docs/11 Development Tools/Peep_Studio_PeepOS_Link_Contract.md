# Peep Studio and PeepOS Link Contract

Status: `active_handoff`

Implementation status: `Stage_5_platform_foundation_pending_HW6_proof`

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
| input | logical A, B, L, R, JOY_LEFT, JOY_RIGHT, JOY_UP, and JOY_DOWN sources |
| visuals | package-backed native-scale masked 1bpp sprite frames |
| retained render model | bounded ordered scene elements with binary alpha and four platform planes |
| waiting animation | authored 250 ms timelines, mixed 2-phase and 3-phase elements, combined timeline compilation, deterministic three-step LPBAM fallback |
| awake preview | exact 168x144 package-backed framebuffer with deterministic fake time and side-effect-free scene thumbnails |
| STOP2 | package visuals compiled into LPBAM animation and resumed across wake/STOP2 handoff |
| firmware package proof | embedded `.egg` loads, validates, resolves STATE content, handles input, and renders the same package pixels as host preview |

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

## Not Yet Exposed

The following are planned or incomplete and must be labelled unavailable in
the editor until this document is updated:

- SEQUENCE and PROGRAM scene authoring or execution;
- editable node graph controls beyond the current inspector commands;
- project mutation commands beyond `state.rename`, `route.set_target`,
  `route.set_guard`, and `route.set_action`;
- 4-tone and 16-tone fixed dither asset import;
- maps, tile layers, fonts, rotation, or interpolated scaling;
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
protocol is version `1`; the current service API is version `10`.

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
| `project.save` | persist the current in-memory project manifest and scene records to authored source files |
| `project.undo` | undo the last accepted command within the bounded service history |
| `project.redo` | redo the last undone command within the bounded service history |
| `project.scene_thumbnails` | return one side-effect-free initial framebuffer snapshot per compiled STATE scene |
| `project.preview_reset` | start one selected STATE scene directly |
| `project.preview_input` | inject one supported logical input source |
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
export `.egg`. The next GUI milestone is editor-owned project navigation and a
read-only scene/graph inspector before mutation commands are introduced.

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

Implementation status: started. Service API version 10 exposes
`project.apply_commands` with the first accepted commands,
`state.rename`, `route.set_target`, `route.set_guard`, and
`route.set_action`, plus `route.add_scene_exit` for creating an actionless
direct scene exit from an unused logical input source to another STATE scene,
and `editor.scene_flow.set_node_position` for editor-only scene-flow layout.
`route.set_action` edits existing ordered route actions only; adding, removing,
and reordering actions remain deferred. Accepted commands update the in-memory
Python-owned project, return a new `project_revision`, normalized document,
diagnostics, and dirty state, and reject stale revisions.
`project.save` persists the current in-memory project manifest and scene records
back to their authored JSON files and clears dirty state. Peep Studio also
exposes Save As for copying the current `.peepproj`
directory to a user-chosen location; checked-in examples opened through the
example button are temporary copies. `project.undo` and `project.redo` keep a
bounded 32-step command history in the Python service; new edits clear redo, and
dirty state is computed against the last saved semantic project hash.

### Stage 3: Scene Canvas And Visual Elements

- add, remove, select, move, and reorder retained visual elements;
- edit bounds, platform layer, focus ownership, sprite/frame reference, and
  animation reference;
- add an asset browser for the implemented native masked-1bpp PNG subset;
- add editable text elements that are deterministically rasterized to masked
  1bpp package pixels at build time;
- keep spatial placement on the scene canvas rather than the behavior graph.

This is the first stage at which the user can begin laying out real menu
screens. Build-time text rasterization is the initial menu-label path; it does
not require or imply an on-device runtime font renderer.

### Stage 4: STATE Graph Editing

- create, remove, rename, and select STATE nodes;
- edit the one declared entry state;
- present local logic as state/prefab node cards with trigger output rows, not
  as a UML-first edge editor;
- create and remove deterministic state-transition outputs and their single
  destination edges through Python service commands;
- edit logical button/joystick routes, guard expressions, ordered actions, and target
  state through typed inspectors;
- support prefab-backed menu nodes through declared slots and read-only generated
  internals before exposing arbitrary custom internals;
- validate unresolved targets and bounded-capacity limits before save/export.

At the end of this stage, an author can build a complete interactive menu that
fits within one STATE scene.

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
routes now accept exactly one `target_state` or `target_scene`; service API 8
can switch an existing actionless route between those targets; selected-scene
preview follows a scene target; and the compiler emits PKG1 `STG1` version 2.
Direct replacement enters the destination entry state, resets destination-local
variables, and starts a new timeline epoch. Peep Studio may expose these exact
semantics for the HW6 FW0 target profile. It must not expose cross-scene route actions,
push/pop, return stacks, or STATE-to-SEQUENCE/PROGRAM transitions yet.

Peep Studio implementation status: first package scene-flow view is
implemented. It presents scenes as separate package-level nodes and shows
existing `target_scene` route edges from normalized service data. Scene cards
show their existing scene-exit outputs as selectable rows and render real
Python-generated initial scene thumbnails through `project.scene_thumbnails`.
Existing actionless `target_scene` routes can be
retargeted to another STATE scene through the inspector. New scene-flow exits
can be added from scene cards through `route.add_scene_exit`; drag-to-create
graph wiring remains deferred, but must call the same Python command rather than
editing JSON in React. Scene-flow cards can be manually rearranged; saved
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

- add formal font and localization package records when required;
- add fixed 4-tone and 16-tone dither profiles;
- add maps, tile layers, scaling, transforms, and richer retained rendering;
- add SEQUENCE authoring, then PROGRAM authoring, only after their runtime and
  package contracts are implemented.

### Menu Authoring Availability

| Delivery point | What the user can author |
|---|---|
| current shell | inspect, run, validate, build, and export an existing STATE project |
| after stage 3 | visually compose static menu screens with sprites and build-time text |
| after stage 4 | create functional interactive menus within one STATE scene |
| after stage 5 | create complete multi-scene menu and navigation hierarchies |
| after stage 6 | visually author and budget menu waiting animations |

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
- the animation timeline owns phase visuals and cadence.

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
