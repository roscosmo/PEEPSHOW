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

| Game behavior | PeepOS runtime class |
|---|---|
| ambient slime and yielded waiting visual | `LP_GRAPH` |
| reactive care menu | `LP_GRAPH` |
| bounded feed transaction | `LP_MODULE` |
| short play microgame | `RT_SCENE` |
| result and return to ambient | `LP_GRAPH` |

This mapping validates the architecture; it does not constrain the eventual full Reference Game to this exact content flow.

## Need To Define

- idle behavior
- movement
- interaction
- fail states
- save points
- return from sleep
