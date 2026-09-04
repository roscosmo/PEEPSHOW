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
  refinement later;
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

Future scene-flow mode:

- left-to-right storyboard graph for moving between scenes or major screens;
- scene nodes should show the scene name, a small screen preview, a card-level
  entry affordance, and one dynamic scene-exit row per declared scene exit;
- adding a scene-flow exit creates the matching local scene-exit in that scene's
  Local Logic graph, and creating a local scene-exit exposes the matching exit
  in Scene Flow. These are the same authored link viewed from two workspaces;
- nodes should show small screen previews, not raw scene IDs;
- direct STATE-to-STATE scene replacement is now exposed for the proven HW6
  target-profile subset; push/pop, return stacks, and other scene types remain
  unavailable until the service exposes them.

Placement mode:

- left: project and scene list;
- center: large scaled screen preview;
- right: inspector for selected visual elements and placement controls.

Until visual element mutation exists, placement mode is a preview-focused shell
for the future placement workflow.
