# Peep Studio and PeepOS Link Contract

Status: `active_handoff`

Implementation status: `Stage_1_read_only_scene_inspection_connected`

This document is the working boundary between Peep Studio development and
PeepOS/HW6 bring-up. It tells an editor agent what the platform actually
supports today, which host interface to use, and which planned features must
not be presented as implemented.

Related:

- [[Authoring_Tool_Architecture]]
- [[Authoring_Project_Schema_Contract]]
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

## Current Proven Capability

The following path is implemented and has been exercised on HW6:

| Area | Current capability |
|---|---|
| editable source | directory-based `.peepproj` project with JSON scene and asset sources |
| package output | deterministic `.egg` binary with SHA-256 integrity |
| scene type | STATE |
| state execution | bounded variables, input routes, guards, actions, and deterministic transitions |
| input | logical A, B, L, and R sources |
| visuals | package-backed native-scale masked 1bpp sprite frames |
| retained render model | bounded ordered scene elements with binary alpha and four platform planes |
| waiting animation | authored 250 ms timelines, mixed 2-phase and 3-phase elements, combined timeline compilation, deterministic three-step LPBAM fallback |
| awake preview | exact 168x144 package-backed framebuffer with deterministic fake time |
| STOP2 | package visuals compiled into LPBAM animation and resumed across wake/STOP2 handoff |
| firmware package proof | embedded `.egg` loads, validates, resolves STATE content, handles input, and renders the same package pixels as host preview |

Measured hardware behavior, current SRAM4 admission limits, and power figures
remain hardware evidence. The desktop preview must not claim to reproduce
current draw or prove STOP2 behavior.

---

## Not Yet Exposed

The following are planned or incomplete and must be labelled unavailable in
the editor until this document is updated:

- SEQUENCE and PROGRAM scene authoring or execution;
- editable node graph commands and project mutation;
- undo/redo command history;
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
protocol is version `1`; the current service API is version `3`.

| Operation | Purpose |
|---|---|
| `service.hello` | discover service/API versions and supported operations |
| `service.shutdown` | request orderly sidecar shutdown |
| `project.load` | load and validate one `.peepproj` directory |
| `project.validate` | return authoritative issues and semantic hash |
| `project.normalize` | return canonical normalized project data |
| `project.build_package` | compile authoritative `.egg` bytes and compatibility report |
| `project.compatibility_report` | inspect target/resource compatibility without exporting |
| `project.preview_reset` | start one selected STATE scene directly |
| `project.preview_input` | inject one logical A/B/L/R input |
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
- create and remove deterministic state-transition edges;
- edit logical A/B/L/R routes, guard expressions, ordered actions, and target
  state through typed inspectors;
- validate unresolved targets and bounded-capacity limits before save/export.

At the end of this stage, an author can build a complete interactive menu that
fits within one STATE scene.

### Stage 5: Package Scene Flow

- create and remove package scenes;
- provide a package-level flow view whose nodes are scenes, not states;
- author declared scene-transition routes, entry behavior, return behavior,
  `transition_scene`, and `exit_to_shell` actions;
- validate every scene target and package entry scene;
- require corresponding PeepOS multi-scene dispatch support before enabling
  export of these routes.

At the end of this stage, an author can build a multi-screen menu hierarchy.
Scene-flow editing must remain separate from the STATE graph inside each scene.

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
- the per-scene **STATE graph** owns the entry marker, states, and deterministic
  state-transition edges;
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
