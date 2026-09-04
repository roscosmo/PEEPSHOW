# Peep Studio UX Direction

Status: `active_direction`

Peep Studio must feel like a simple visual state-machine authoring tool, not a
JSON/schema editor. The Python authoring service remains the semantic authority,
but the GUI should translate project records into user-facing concepts.

## Product Shape

- The main workspace is the scene logic graph: states and transitions should get
  the largest area by default.
- The local logic graph should feel like a node/shader editor, not a UML state
  chart. State and prefab nodes own their trigger outputs inside the node card,
  and graph lines connect from those outputs to their single destination.
- The display preview belongs at the top of the inspector during normal logic
  editing, scaled to fit without scrollbars.
- A placement mode should promote the display preview into the main workspace so
  visual elements can later be selected, moved, aligned, and ordered.
- Low-level field names such as `route`, `set_variable`, `variable_ref`, and
  `request_render` should not be the default authoring language.
- Advanced/internal labels may remain available in debug details, tooltips, or a
  later advanced view, but the primary editor should use plain language.

## Design Source And Review

The active node-design reference may be a shared Excalidraw board. When a board
is used, the designer should attach or export the `.excalidraw` source, a PNG,
or both before implementation starts. The current live reference is:

- `docs/11 Development Tools/Peep Studio Design/Peep-Studio_Design-notes.excalidraw`

Peep Studio work should treat that drawing as the visual design source and these
docs as the behavior source. Before changing graph interaction patterns, the
agent should summarize the drawing into concrete implementation notes covering:

- node-card anatomy;
- connection zones and handle behavior;
- where triggers, destinations, and transition effects are visually separated;
- drag, selection, and delete behavior;
- inspector responsibilities;
- compact and desktop layout expectations.

The agent should confirm any unclear interaction in plain English rather than
guessing from the drawing.

## User-Facing Vocabulary

- State: a named screen or moment in the interaction.
- Prefab node: a reusable self-contained authoring block, visually distinct from
  a custom state node.
- Transition: what moves from one state to another.
- When: the input that triggers a transition.
- Only if: conditions that must be true before the transition can happen.
- Then: effects that happen when the transition runs.
- Screen: the 168x144 visual output for the selected state.
- Output: a routable result exposed on the right side of a node card. Each
  output connects to at most one destination.

## Local Logic Graph Target

Peep Studio should present local STATE logic as dynamic node cards:

- a new custom state node starts with no exits;
- choosing a trigger, such as Button A, adds a visible exit row to that node;
- state entry is a card-level connection affordance, not a separate editable
  entry row. The visual design may show green oval entry handles around the
  top/bottom/corner entry zones, but these handles only mean "a transition can
  arrive here";
- side edges are reserved for exit rows. The editor should choose the left or
  right side of each exit automatically to reduce line overlap, with manual
  refinement available;
- automatic transition routing prefers the shortest clear orthogonal path.
  Node intersections, overlapping lines, backtracking, excess bends, and path
  length are penalized in that order. An outside loop is only appropriate when
  a compact route is obstructed;
- manually adjusted transition rails remain authoritative when connected nodes
  move. Endpoint segments may be re-anchored or simplified when invalid, but a
  small movement elsewhere must not reset the author's route;
- State Graph transitions use the Excalidraw language of thick blue dashed
  paths with solid blue direction arrows. The green Scene Entry connection is
  distinct and remains an entry affordance rather than a triggered transition;
- each exit row summarizes the trigger and shows condition badges, such as
  "Button A - 1 condition";
- effects that happen during a transition should be shown on the transition
  line as a compact transition-effect chip/node where useful. This separates
  "what caused the move" from "what changed during the move";
- condition and effect chips may be dragged anywhere along the transition line.
  Their visual order follows guard evaluation and action execution order, so
  adjacent chips constrain one another and cannot be dragged past each other;
- outputs are grouped by trigger type where useful: buttons, timers,
  variable/condition events, and system events;
- each exit has a single destination to preserve state-machine clarity;
- selecting a state card in Local Logic pauses and restarts the live emulator
  from that state. The emulator's current state uses a dedicated runtime
  highlight that remains visually distinct from author selection and follows
  transitions without moving the author's selection;
- while the emulator runs, the State Graph viewport smoothly centers each new
  runtime-active state without changing the current zoom, node positions,
  manual route layout, or author selection;
- Reset returns to the state from which the current preview run was launched.
  Selecting a scene instead launches from its declared entry state;
- advanced/internal route IDs remain available for debugging, but are not shown
  on the node face by default.

Semantic transitions still compile through the Python authoring service as
bounded routes, guards, actions, and target states. React must not create
graph-only behavior that the Python service cannot validate or compile. Visual
transition-effect chips are presentation of existing service-owned route
actions. Their manually adjusted path positions are typed editor-only metadata;
they never change route semantics or compiled package output.

State cards may use badges for author-facing summaries:

- **Variables**: local state variables changed or tested by the state's exits,
  such as counters, menu selection, gold, age, or flags.
- **Objects**: retained scene objects changed by the state's entry behavior or
  transition effects, such as sprites, text sprites, primitive shapes, position,
  visibility, frame choice, or animation choice.

Exact badge counts should come from normalized service data or service-provided
summary fields. React should not infer package semantics from raw JSON in ways
the Python service cannot validate.

## Hierarchical State UX Target

A composite state groups one active child region and owns handlers shared by
its descendants. In a menu, Start Game, Settings, and Credits can be child
states of Menu Selection while Menu Selection owns one R handler. The graph
must not display or persist three generated copies of that parent handler.

Event behavior is presented in author language:

- the active child gets the trigger first;
- an enabled child handler consumes it;
- otherwise the trigger is offered to its parent;
- a parent action that does not change state leaves the selected child active;
- an explicit **Block inherited trigger** handler consumes an event without an
  action when a child must suppress the parent fallback.

Composite-state presentation must follow the live Excalidraw design language.
Before implementation, the design pass must choose whether children expand
inside the parent card, open as a focused nested canvas with breadcrumbs, or
support both. Regardless of that choice:

- the scene hierarchy remains visible and identifies the active state path;
- a composite card visibly differs from a leaf without resembling a prefab;
- parent-owned triggers remain visible on the composite card;
- a focused child graph shows inherited parent handlers without duplicating
  editable rows;
- selecting a parent handler opens one inspector record and one action order;
- moving or laying out child nodes is editor-only and does not alter hierarchy;
- the emulator reports the complete active path, not only the leaf label.

History controls use plain labels rather than requiring UML terminology:

```text
On re-entry
  Start at initial substate
  Remember previous substate
  Remember complete nested state
```

The inspector may disclose shallow/deep history terminology in advanced help.
Package launch remains fresh unless a later package-resume contract explicitly
says otherwise.

Scene Flow navigation must distinguish semantic behavior:

- **Go to** replaces a scene;
- **Go to and resume** restores remembered destination state;
- **Open** retains the current scene context;
- **Return** resumes the retained context.

These labels cannot be implemented as renderer-only variants of the existing
editor Go To reference card. They require typed service-owned navigation data
and target-profile capability support.

### Hierarchical Bring-Up Plan

1. **Contract and capability slice**: agree on event bubbling, internal-handler
   behavior, history fallback, navigation modes, runtime bounds, and suspended
   timer/audio policy. Publish capability fields before enabling controls.
2. **Service model and validation**: add composite/parent records, initial-child
   rules, internal handlers, cycle checks, deterministic ordering, history
   validation, and normalized inherited-handler summaries.
3. **Compiler and preview parity**: flatten hierarchy into bounded dispatch
   tables, expose the active path in traces, and prove child-first dispatch and
   no-reentry internal actions with deterministic tests.
4. **Local Logic UI**: create/reparent states, enter and leave a composite,
   author parent handlers once, inspect inherited behavior, and preserve nested
   editor layouts. Resolve the exact canvas modality against Excalidraw first.
5. **State history**: expose initial, shallow, and deep re-entry choices only
   after service preview and compiled execution agree.
6. **Restorable scene navigation**: add Go to and resume, Open, and Return after
   bounded runtime scene contexts and suspend/resume rules are exposed.
7. **Acceptance example and hardware gate**: rebuild the menu example so R is a
   parent handler, Settings/Credits return to the previous selection, package
   launch starts at selection one, and the same trace passes service, Studio,
   package-build, and current-HW6 tests.

Do not start with prefab abstraction. A future Menu prefab may generate or own
this hierarchy only after ordinary composite states, inherited dispatch,
history, and restoration are robust and inspectable.

## Prefab And Menu Nodes

Users should not need to build common menus from empty low-level states.
Reusable menu behavior belongs in prefab-backed nodes and Authoring Kits. This
is an intended direction, not the immediate implementation target. Peep Studio
must first make ordinary STATE logic, placement, assets, scene flow, and package
export robust through the Python service before prefab-backed authoring is made
editable.

A menu prefab node should be self-contained and customizable through slots:

- menu type;
- item labels and item count;
- selected-item variable;
- selection marker visual;
- default navigation input template;
- confirm/cancel behavior;
- one routable output per menu item choice.

Navigation inside a standard menu prefab is handled by the prefab by default.
The node can show greyed-out generated internals in the inspector so authors can
understand what is happening, but those internals are not directly editable while
the node remains prefab-backed.

Prefab outputs may represent local logic or a future scene transition. Scene
transition outputs belong in the package scene-flow graph and must remain
disabled for export until PeepOS supports multi-scene dispatch.

Authors who need behavior outside the prefab slots should create a custom node
or copy/convert the prefab into custom editable STATE records.

Prefab internals must come from typed service-owned records. The renderer must
not special-case the current example project, object names, scene names, or demo
focus roles to simulate prefab behavior.

## Action Authoring Target

Raw actions should be presented as readable effects:

- `set_variable` with `add`: "Change [counter] by [+1]".
- `set_variable` with `assign`: "Set [counter] to [1]".
- `play_sfx`: "Play sound [sound name]".
- `request_render`: backend-only refresh work; it should not be shown as an
  author effect.

The Stage 2 command layer may continue editing existing action records only, but
the UI should increasingly present those records as friendly effect rows,
presets, and pickers.

## Sound Authoring Target

Sound controls belong in the Assets workspace and Local Logic inspectors, not in
Placement.

- Assets should present package-backed sampled SFX as **Sounds**.
- Import should accept supported WAV sources and create an author-facing sound
  plus its default cue through the Python service.
- The sound card should show duration, packaged size, volume, and audition.
- Audition plays the exact compiled package bytes on the host. It is useful for
  content checking, but it is not HW6 timing, power, or speaker-quality proof.
- Local Logic and Scene Flow should expose sound playback as **Play sound**.
- Music, looping, procedural audio, and arbitrary mixing should be visibly
  unavailable until the service and target profile expose them.

## Layout Target

Normal logic mode:

- left: project and scene list;
- center: large per-scene state graph;
- right: wider inspector with screen preview at the top, then selected-state or
  selected-transition controls.

Scene-flow mode:

- left-to-right storyboard graph for moving between scenes or major screens;
- scene nodes use the shared Excalidraw visual language: a centered scene name,
  dominant screen preview, attached green scene-entry capsule, aligned
  scene-exit stems and nodes, and a dotted **Add new exit** row;
- scene cards do not show an internal-state count badge. State counts become
  misleading as prefab-backed authoring replaces exposed low-level states;
- Package Entry is a standalone movable graph node with exactly one output.
  Connecting that output to a scene entry changes the package entry scene;
- reusable **Go to <scene>** nodes are editor-only visual references to an
  existing scene. Multiple scene exits may connect to one reference so a
  backward semantic jump can still read left-to-right in the storyboard;
- deleting a Go To reference removes only the visual alias. Attached scene
  exits retain their authored destination and fall back to the real scene node;
- adding a scene-flow exit creates the matching local scene-exit in that scene's
  Local Logic graph, and creating a local scene-exit exposes the matching exit
  in Scene Flow. These are the same authored link viewed from two workspaces;
- nodes should show small screen previews, not raw scene IDs;
- selecting a scene opens a scene-specific inspector with editable display
  name, entry-state summary, scene type, declared exits, and stable ID. Package
  Entry and Go To nodes have their own contextual inspectors;
- initial connections use automatic left-to-right orthogonal routing. Manual
  Scene Flow route refinement follows after the node language is stable;
- direct STATE-to-STATE scene replacement is now exposed for the proven HW6
  target-profile subset; push/pop, return stacks, and other scene types remain
  unavailable until the service exposes them.

Placement mode:

- left: the fixed emulator/display preview, scene hierarchy, then a contextual
  placement hierarchy for the selected scene;
- center: large scaled screen preview;
- right: inspector for selected visual elements and placement controls.

The placement hierarchy mirrors semantic ownership rather than flattening the
resolved framebuffer:

```text
Scene
  Base Placement
    scene-owned objects
  States
    State A
      objects changed by State A
    State B
      objects changed by State B
```

`Base Placement` is a first-class edit scope. State rows contain only local
object variations; they do not imply separate screens. The same stable object
may appear under Base Placement and under every state that changes it. With
hierarchical states, child state rows nest beneath their composite parent and
inherit the parent's placement changes.

Placement target selection follows conventional tree-selection behavior:

- plain click selects one state exclusively and makes it the primary preview;
- Ctrl-click toggles individual state scopes;
- Shift-click selects a contiguous range of sibling states;
- Ctrl+A selects all state siblings in the focused hierarchy level;
- the most recently clicked selected state remains the primary preview while
  edits apply to the complete selected set;
- clicking Base Placement clears state selection and previews/edits the scene
  defaults;
- Base Placement and state scopes cannot be selected together;
- an ancestor and its descendant cannot both be edit targets because the
  ancestor already applies through inheritance.

There are no state-target checkboxes and ordinary row clicks never accumulate
targets. Selecting all currently declared states remains an explicit fixed set;
it is not normalized to Base Placement and does not include states created
later.

Adding an object uses the selected scope:

- Base Placement creates a scene-wide object inherited by current and future
  states;
- one selected state creates an object visible in that state scope;
- multiple selected states create one stable object visible in exactly those
  scopes;
- selecting a composite parent makes the object visible through that parent's
  descendants, including future descendants.

The inspector shows a read-only edit-target breadcrumb instead of an
"Applying edits to" checkbox panel. Each editable property identifies whether
its value is defined here or inherited, identifies the inherited source when
useful, and provides a reset action for removing the local override. Resetting
an override must reveal the inherited value; it must not write a copied value
back into the state.

When multiple states are selected, the hierarchy clearly distinguishes the
primary preview state from the complete edit selection. The center canvas shows
the resolved primary state while each mutation is applied atomically to the
explicit selected-state set.
