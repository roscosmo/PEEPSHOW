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

- a new custom state node starts with no outputs;
- choosing a trigger, such as Button A, adds a visible output row on the right
  side of that node;
- each output row summarizes the trigger and shows condition/effect badges,
  such as "Button A - 2 rules - 1 effect";
- outputs are grouped by trigger type where useful: buttons, timers,
  variable/condition events, and system events;
- each output has a single destination to preserve state-machine clarity;
- advanced/internal route IDs remain available for debugging, but are not shown
  on the node face by default.

Semantic transitions still compile through the Python authoring service as
bounded routes, guards, actions, and target states. React must not create
graph-only behavior that the Python service cannot validate or compile.

## Prefab And Menu Nodes

Users should not need to build common menus from empty low-level states.
Reusable menu behavior belongs in prefab-backed nodes and Authoring Kits.

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

## Action Authoring Target

Raw actions should be presented as readable effects:

- `set_variable` with `add`: "Change [counter] by [+1]".
- `set_variable` with `assign`: "Set [counter] to [1]".
- `request_render`: "Refresh the screen".

The Stage 2 command layer may continue editing existing action records only, but
the UI should increasingly present those records as friendly effect rows,
presets, and pickers.

## Layout Target

Normal logic mode:

- left: project and scene list;
- center: large per-scene state graph;
- right: wider inspector with screen preview at the top, then selected-state or
  selected-transition controls.

Future scene-flow mode:

- left-to-right storyboard graph for moving between scenes or major screens;
- nodes should show small screen previews, not raw scene IDs;
- this mode remains a bring-up target, but authoring/export must stay disabled
  until PeepOS has corresponding multi-scene dispatch support.

Placement mode:

- left: project and scene list;
- center: large scaled screen preview;
- right: inspector for selected visual elements and placement controls.

Until visual element mutation exists, placement mode is a preview-focused shell
for the future placement workflow.
