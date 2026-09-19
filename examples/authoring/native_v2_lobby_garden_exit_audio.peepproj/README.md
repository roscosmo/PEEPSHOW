# Native V2 Lobby/Garden Scene-Exit Audio Fixture

API 50 integration fixture for scene-exit SFX through the normal Studio export
path. It is intentionally separate from the previously hardware-tested silent
and local/timer-audio Lobby/Garden fixtures.

## Expected controls

- Lobby is static. `A` enters Garden and dispatches one long, recognizable cue
  only after the destination is admitted and committed.
- Garden starts in the clearly labelled LEFT slot.
- `L` selects LEFT and `R` selects RIGHT by changing only the selection marker;
  these local transitions are silent.
- The numbered `1-2-3-4` animation loops at 400 ms per frame and must not restart
  when the selected slot changes.
- The TIMER indicator fills once, two seconds after entering Garden. Slot changes
  must not restart or delay it. The timer handler is silent.
- `B` returns to Lobby with one short cue. If the long cue is still active, it
  continues while the short cue starts.
- Inapplicable `A` in Garden and `B` in Lobby do nothing and produce no cue.
- Every fresh Garden entry resets to LEFT, restarts the numbered animation at
  frame 1, hides the timer fill, and starts a fresh two-second timer.

`HOLD START` must stop and discard active package audio; Resume must stay silent.
The project contains no local/timer audio, calendar-time logic, parallel regions,
remembered scene state, runtime playback controls or development-encoder workaround.
