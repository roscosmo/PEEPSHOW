# Studio Variable Scope and Scene Entry Handoff

Date: 2026-09-19. Design handoff only; no service API bump, new command names,
schema fields, runtime support or export capability is delivered here.

Follow-up: [[Peep_Studio_Scoped_Variables_API52_Handoff]] now defines the delivered
opt-in variable authoring/preview subset. Entry graphs remain design-only, and
scoped-variable firmware/export support is still blocked.

Authoritative design: [[Variable_Scope_and_Scene_Entry_Design]]. This replaces
the earlier special Remembered State entry-node proposal, not existing scene
timer, calendar or SFX behavior.

## Agreed Authoring Model

- Variables have explicit Scene or Package scope, boolean or bounded-integer
  type, and an initial value. Integers also have declared minimum/maximum bounds.
- Scene values initialise on fresh scene entry; package values initialise at
  package-session start and survive scene changes. Both survive pause/resume.
- Restart/reset starts a new game session. Package scope is not flash persistence.
  Retained system time does not retain game values; world time continues on pause.
- A scene accesses only its own variables and package variables. References use
  stable scope-qualified IDs; names are author-facing labels, not lookup rules.
- Variable actions are set, increment/decrement by and reset to initial. Integer
  arithmetic clamps to bounds. No global variable-reset command is required.
- Fresh scene entry has one visible path: actions, ordered conditions with a
  mandatory default, branch/shared actions, then a local state. First match wins.
  Branch order must be explicit, not inferred from screen position.
- Checks and actions see preceding staged writes on that selected path. Branches
  may merge but do not execute in parallel. Entry paths cannot loop or wait.
- No state-entry action lists, automatic state remembering, secret variables,
  implicit scene resume or variable-change polling are introduced.

Example: selection transitions write package variable `main_menu_selection`;
menu entry checks it to select a local state, with a visible fallback. This is
ordinary authored logic, not a special remembering toggle.

## Transaction and Preview Expectations

Supported outgoing writes and destination entry execution are staged together.
Destination checks see those staged package values and freshly initialised scene
values. Commit only after destination admission succeeds; publish SFX afterward.
A rejected entry must not leave package writes or emitted sounds behind. Valid
exported scenes should normally enter; build validation must catch predictable
failures rather than presenting admission failure as ordinary gameplay.

Entry actions run only on fresh entry. Local transitions and shell resume do
not execute the scene-entry path. Existing state overrides and timer lifetimes
remain explicit; full animation/timer/instance resume is a separate future feature.

## Work Split

Studio can develop UX proposals now: scope labels, variable selectors, entry
actions, ordered branch/fallback presentation and merges. Do not persist guessed
fields, compile local substitutes, or enable unsupported runtime/export controls.

OS/backend next defines the concrete shared records, reference checks, editing
commands, resource limits, preview behavior and capability advertisement. Exact
entry action support must be enumerated; the design does not enable all existing
transition actions on entry. Variable and graph capacities must include staged
transaction storage, not just committed values.

Preserve the current direct `entry_state` behavior for existing projects. Shared
commands must cover save/reopen, undo/redo and reference-safe deletion. Studio
consumes shared diagnostics and budgets rather than calculating alternate rules.

Keep ongoing API 51 calendar and scene-exit SFX integration separate and usable.
The new memory design does not block verification of those delivered features.

## First Joint Fixture

After capabilities are implemented, use two scenes to demonstrate explicit menu
selection in a package variable, a scene-local value that resets on fresh entry,
ordered entry checks/actions, and a visible fallback. Verify shell Resume keeps
both values while Restart resets both. Include build rejection cases and failed
destination admission rollback before claiming the new feature is complete.
