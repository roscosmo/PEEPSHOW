# Authoring Tool Architecture

Status: `accepted_design`

Implementation status: `partial`

The versioned newline-delimited JSON protocol, project revision checks, and
long-running Python service are implemented. The service exposes project load,
validation, normalization, deterministic in-memory package compilation, a V1
compatibility report, and deterministic selected-STATE-scene preview over the
independently parsed package. The first Electron, TypeScript, and React shell is
implemented with a sandboxed renderer, selected-scene panel preview, logical
input controls, runtime inspection, package build, and `.egg` export. Project
mutation, undo/redo, and visual graph editing remain open.

This document defines the host application architecture for the PeepShow game
authoring tools. The provisional working name for the desktop application is
**Peep Studio**. It governs the desktop editor, authoring preview, Python
toolchain boundary, and deterministic `.egg` build path; the working name does
not freeze final product naming.

Related:

- [[Development_Tooling_Index]]
- [[Authoring_Project_Schema_Contract]]
- [[HW6_Authoring_Vertical_Slice]]
- [[Peep_Studio_PeepOS_Link_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[Package_Compatibility_Report_Contract]]

---

## Purpose

The authoring application must provide a responsive visual editor without
creating a second package compiler or a second interpretation of PeepOS game
semantics.

The approved V1 architecture is:

```text
Electron desktop shell
        |
TypeScript + React renderer
        |
versioned narrow IPC bridge
        |
Python authoring service
        |
project load/save, validation, normalization, preview, compilation
        |
temporary in-memory package / installable .egg
        |
host authoring preview or PeepOS package runtime
```

The desktop UI is reactive presentation. The Python authoring service remains
the authority for source-project semantics, validation, normalization,
capability closure, package compilation, and reference preview execution.

### Current STATE Vertical Slice

The immediate editor milestone is deliberately narrower than the complete
rendering and game-authoring contracts. It must prove this path first:

```text
masked 1bpp PNG + binary alpha
        |
.peepproj STATE scene, states, transitions, placements, and animations
        |
Python validation and deterministic .egg compilation
        |
selected-scene HOST_AUTHORING_PREVIEW
        |
the same .egg asset and scene records on PeepOS
```

This milestone includes only the asset and runtime behavior needed to author,
preview, build, and run a package-backed `STATE_SCENE`. It does not require
tone/dither import, Tiled maps, fractional transforms, SEQUENCE, PROGRAM,
audio, or the measured HW6 digital twin. Their schema namespaces and contracts
remain extensible, but none may be implemented as an editor-only shortcut or a
second package interpretation.

---

## Technology Decision

The V1 desktop application uses:

| Area | Technology | Responsibility |
|---|---|---|
| desktop shell | Electron | windows, menus, lifecycle, file dialogs, sidecar lifecycle, device-tool integration |
| visual UI | TypeScript and React | panels, inspectors, editor interaction, diagnostics, preview presentation |
| behavior graph | React Flow | node/edge interaction, selection, pan/zoom, connection editing, graph layout metadata |
| scene and panel preview | HTML Canvas 2D | pixel-accurate `168 x 144` monochrome presentation and retained-element editing |
| authoring core | Python | `.peepproj` parsing, schema validation, normalization, compilation, compatibility reporting, reference simulation |
| firmware runtime | C / PeepOS | authoritative target execution of validated `.egg` package data |

Electron is selected over a Python GUI framework because the editor needs a
rich multi-panel application, custom canvases, graph interaction, and a
long-lived reactive state model. Python remains behind a process boundary so
the existing headless compiler and tests stay authoritative.

Tauri is not selected for V1. It would add Rust and cross-platform sidecar
packaging before those costs solve a demonstrated problem. The React renderer
and IPC contract must remain shell-independent enough that a future shell
change does not alter project or package semantics.

Java, C#, Unity, Godot, and custom native GUI stacks are not selected for V1.
They would either require a second language/runtime boundary around the
existing Python compiler or couple the PeepShow editor to an unrelated game
engine.

---

## Process Boundaries

### Renderer Process

The React renderer owns only user interaction and presentation state.

It may own:

- panel arrangement and selected tabs
- graph viewport and node positions
- current selection and hover state
- unsaved-edit indicators
- cached read-only operation results
- display scaling and preview controls

It must not:

- compile `.egg` packages directly
- independently decide target compatibility
- access arbitrary host files
- spawn processes
- bypass schema validation
- implement a separate package format

### Electron Main Process

The Electron main process owns desktop privileges and the Python service
lifecycle. The renderer accesses these through a narrow, context-isolated
preload API.

The main process owns:

- project open/save dialogs
- approved host filesystem operations
- Python service start, stop, timeout, and crash reporting
- package export destinations
- future device installation command routing

### Python Authoring Service

The Python service wraps the existing `peepshow_authoring` package. It owns the
canonical open project document for a session and exposes versioned operations.

Implemented V1 operations are:

```text
service.hello
service.shutdown
project.load
project.validate
project.normalize
project.build_package
project.compatibility_report
```

Reserved operations for later milestones include:

```text
project.new
project.save
project.apply_commands
project.undo
project.redo
project.preview_reset
project.preview_input
project.preview_advance
```

The three preview operations are implemented in service API version 3. Their
payloads identify a selected scene directly, so an author can test any STATE
scene without playing from the package entry scene. Scene-direct launch is an
editor/debug fixture only and is not serialized as package gameplay behavior.
Reset compiles and independently parses an immutable in-memory `.egg`; input
and time operations then execute only those parsed records.

Reserved operations must not be advertised by service discovery until their
semantics and tests are implemented.

Requests and responses use a versioned newline-delimited JSON protocol over
standard input/output. Each request has an ID, operation, schema version, and
typed payload. Service diagnostics and preview frames are explicit events.

No localhost server or dynamically selected network port is required. Package
compilation returns deterministic package bytes and their matching compatibility
report to the privileged desktop process; the service does not choose an export
path or silently write an installable artifact.

---

## Canonical Data Flow

The editable source remains a `.peepproj` directory. The installable result
remains a deterministic `.egg` file.

```text
.peepproj source
        |
load and schema validation
        |
normalized authoring model
        |
semantic validation and target closure
        |
temporary package bytes for preview
        +----------------------+
        |                      |
host authoring preview     save identical build path as .egg
                               |
                         install on PeepShow
```

The preview must consume normalized or compiled package semantics from the
Python toolchain. It must not animate raw editor records using UI-only rules.

Changing editor-only metadata such as node positions, comments, panel layout,
or selection must not change package output.

---

## Preview And Digital Twin Terminology

The first editor simulator is `HOST_AUTHORING_PREVIEW`.

It provides:

- deterministic fake time at the authored cadence
- A/B/L/R, joystick cardinal, and declared event injection
- STATE execution, variables, routes, guards, and actions
- retained visual composition
- waiting-animation preferred and fallback previews
- target capability and SRAM4 admission diagnostics
- deterministic save-data simulation where required

It is not hardware evidence and must be labeled as a preview.

`HOST_DIGITAL_TWIN_HW6` is a later measured model. It may use the same UI, but
it cannot be declared complete until it reproduces validated HW6 timing,
fallback, suspend/resume, wake, clock, and power behavior within documented
tolerances.

---

## Editor Surfaces

The editor uses separate views for separate authoring concerns.

| Surface | Purpose |
|---|---|
| project browser | packages, scenes, states, prefabs, assets, and parameters |
| scene canvas | spatial placement, bounds, retained layers, focus, and visual selection |
| state graph | STATE and prefab node cards with trigger output rows and routable destinations |
| transition inspector | selected output trigger, guards, ordered actions, and destination |
| property inspector | schema-driven typed properties and target-derived limits |
| animation timeline | phase visuals, authored cadence, and combined waiting sequence |
| authoring preview | pixel-accurate panel output, deterministic controls, and runtime trace |
| build panel | validation errors, warnings, compatibility, resource budget, and `.egg` output |

The graph is a view of semantic project records, not the runtime itself.
Spatial visuals do not belong in the behavior graph. Transition detail does not
belong on the scene canvas. Animation cadence does not belong in node position
metadata.

For the initial STATE editor, the first implementation may expose raw state
nodes and transition edges while the command surface is being proven. The target
authoring model is more user-facing:

- custom STATE records appear as screen/state node cards;
- reusable prefabs appear as visually distinct prefab node cards;
- transition triggers appear as output rows owned by the source node, grouped by
  trigger type where useful;
- each output row has one destination to keep the state machine readable;
- route, guard, action order, and target details are edited through inspectors;
- prefab internals may be shown read-only for understanding/debugging, but
  editable prefab behavior flows through declared slots and settings;
- scene visuals are edited on the scene canvas;
- animation phases are edited on the timeline.

The node-card graph is a presentation of semantic records. It must lower through
the Python service into bounded STATE scenes, variables, routes, guards, actions,
render elements, and package scene records. React may own positions, expansion
state, hover state, and selected output rows; it must not invent behavior that
the Python service cannot validate and compile.

SEQUENCE uses a timeline-first editor. PROGRAM may add richer graph surfaces
only when the bounded runtime contract requires them.

---

## Editing And Undo

Semantic edits use typed commands rather than arbitrary object mutation.

Examples:

```text
state.create
state.rename
transition.create
transition.set_guard
element.move
element.set_layer
animation.set_phase_visual
```

The Python service applies commands, validates affected records, and returns a
new document revision plus diagnostics. Undo and redo operate on the command
history. Editor-only viewport changes may remain local and use a separate UI
history.

Autosave must write source-project recovery data only. It must never silently
replace the last exported `.egg` package.

---

## Responsiveness Rules

- pointer movement, selection, pan, zoom, and panel interaction stay local to
  the renderer.
- semantic validation is incremental where possible and bounded.
- full validation and package compilation may run in the Python service
  without blocking the renderer.
- stale operation results are rejected using project revision IDs.
- preview advances from an explicit clock and never from uncontrolled wall
  time inside the compiler.
- the preview framebuffer is a fixed `168 x 144` monochrome image; nearest-
  neighbor scaling preserves panel pixels.
- compiler or service failure must produce a visible diagnostic and must not
  corrupt the open source project.

---

## Security And Reliability

- Electron context isolation and renderer sandboxing remain enabled.
- the preload bridge exposes named operations, not unrestricted Node APIs.
- the Python service accepts only the versioned protocol.
- project paths are selected or approved by the main process.
- package export uses an atomic temporary-file replacement.
- malformed service output terminates the operation and reports an internal
  tool error.
- the editor cannot mark a package compatible when the compiler rejects it.

---

## Testing

The tool architecture requires:

- Python unit tests for source schema, command application, validation,
  normalization, preview execution, and package compilation
- protocol contract tests for every request, response, event, and version
- deterministic golden tests from `.peepproj` through `.egg`
- preview trace fixtures for input, time, transitions, variables, and waiting
  animation fallback
- React component tests for inspectors and graph command generation
- end-to-end desktop tests for open, edit, preview, build, and save
- target conformance captures comparing selected preview traces with HW6
  runtime evidence

Passing authoring-preview tests does not replace target validation.

---

## Implementation Order

1. freeze the V1 editor-service protocol and project revision rules
2. expose the existing Python loader, validator, compiler, and compatibility
   report through the service
3. implement deterministic masked-1bpp PNG import and compile asset records into
   `asset_table`, `masked_1bpp_sprite_bank`, and `animation_table` chunks
4. selected-STATE-scene reference preview against the compiled package reader,
   with explicit fake time and logical input injection - implemented for package-backed
   native-scale masked 1bpp sprites
5. make firmware consume those same package sprite/frame records instead of a
   compiled-in visual catalog
6. create the Electron, TypeScript, and React shell with the `168 x 144` panel
   preview - implemented
7. add the project browser, scene canvas, STATE graph, transition inspector,
   schema-driven properties, and the minimum animation timeline
   - project browser and read-only runtime inspector implemented; graph,
     properties, and timeline editing remain open
   - the staged delivery order for inspection, mutation, visual composition,
     STATE editing, package scene flow, and animation is defined by
     [[Peep_Studio_PeepOS_Link_Contract]]
8. add deterministic `.egg` export and compatibility/resource diagnostics
   - deterministic export implemented; dedicated resource diagnostics UI remains open
9. connect package installation and activation for rapid HW6 iteration
10. extend the asset pipeline with additional fixed tone/dither profiles, maps,
    transforms, fonts, and richer retained rendering only as vertical-slice
    requirements demand them
11. extend the same scene architecture to SEQUENCE, then PROGRAM
12. admit `HOST_DIGITAL_TWIN_HW6` only after measured conformance closure

The current milestone has reached the first shell portion of steps 6 through 8:
one selected authored STATE scene uses the same compiled masked-1bpp package
pixels, state graph, and animation records in Peep Studio preview and on HW6.
The desktop shell exposes that proven path rather than defining it.

---

## Rule

The visual editor may improve authoring ergonomics, but it must remain a client
of the authoritative schema, validator, compiler, and target profile. UI
convenience must never create package semantics that the headless toolchain
cannot reproduce.
