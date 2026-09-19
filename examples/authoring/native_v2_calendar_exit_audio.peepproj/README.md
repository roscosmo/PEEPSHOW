# Native V2 Calendar and Scene-Exit Audio Fixture

API 51 integration fixture for a local calendar schedule and scene-exit SFX
through the normal Studio export path. It is intentionally separate from the
previously hardware-tested Lobby/Garden fixtures.

## Expected controls

- Lobby is static. `A` enters Garden and dispatches one long, recognizable cue
  only after the destination is admitted and committed.
- Garden starts in the clearly labelled LEFT slot.
- `L` selects LEFT and `R` selects RIGHT by changing only the selection marker;
  these local transitions are silent.
- The numbered `1-2-3-4` animation loops at 400 ms per frame and must not restart
  when the selected slot changes.
- Garden registers one `next_occurrence` schedule for 12:34 local time. The CLOCK
  indicator fills once when that occurrence is delivered. L/R changes preserve
  the schedule and its handler is silent.
- `B` returns to Lobby with one short cue. If the long cue is still active, it
  continues while the short cue starts.
- Inapplicable `A` in Garden and `B` in Lobby do nothing and produce no cue.
- Every fresh Garden entry resets to LEFT, restarts the numbered animation at
  frame 1, hides the timer fill, and starts a fresh two-second timer.

`HOLD START` must stop and discard active package audio; Resume must stay silent.
Calendar time continues while shell-suspended. If 12:34 passes while suspended,
the CLOCK indicator fills after Resume rather than while the package is inactive.
Leaving Garden before delivery cancels its schedule; a fresh Garden entry arms a
fresh next occurrence. The project contains no local/timer audio, parallel regions,
remembered scene state, runtime playback controls or development-encoder workaround.
