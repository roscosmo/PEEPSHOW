# Core Loop

## Main Loop

```text
player observes
player acts
slime acts
world updates
screen updates
state persists
```

## Modes

- ambient
- turn-based
- realtime burst

## First Vertical Slice

The first end-to-end implementation is specified by [[HW6_Authoring_Vertical_Slice]].

Its mode mapping is deliberately small:

| Game behavior | Package scene type | Execution |
|---|---|---|
| ambient slime and yielded waiting visual | `STATE_SCENE` | `REACTIVE` |
| reactive care menu | `STATE_SCENE` | `REACTIVE` |
| bounded feed transaction | `STATE_SCENE` | `REACTIVE` |
| short play microgame | `PROGRAM_SCENE` | `REALTIME` |
| result and return to ambient | `STATE_SCENE` | `REACTIVE` |

This mapping validates the architecture; it does not constrain the eventual full Reference Game to this exact content flow.

## Need To Define

- idle behavior
- movement
- interaction
- fail states
- save points
- return from sleep
