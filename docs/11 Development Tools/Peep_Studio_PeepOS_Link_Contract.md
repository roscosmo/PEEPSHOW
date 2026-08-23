# Peep Studio and PeepOS Link Contract

Status: `active_handoff`

Implementation status: `STATE_vertical_slice_connected`

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
