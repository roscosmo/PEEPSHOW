# Peep Studio UX Direction

Status: `active_direction`

Peep Studio must feel like a simple visual state-machine authoring tool, not a
JSON/schema editor. The Python authoring service remains the semantic authority,
but the GUI should translate project records into user-facing concepts.

## Product Shape

- The main workspace is the scene logic graph: states and transitions should get
  the largest area by default.
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
- Transition: what moves from one state to another.
- When: the input that triggers a transition.
- Only if: conditions that must be true before the transition can happen.
- Then: effects that happen when the transition runs.
- Screen: the 168x144 visual output for the selected state.

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

Placement mode:

- left: project and scene list;
- center: large scaled screen preview;
- right: inspector for selected visual elements and placement controls.

Until visual element mutation exists, placement mode is a preview-focused shell
for the future placement workflow.
