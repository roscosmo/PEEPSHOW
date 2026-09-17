# Native V2 Lobby/Garden Integration Fixture

Source-only Studio and OS integration fixture. Ordinary multi-scene V2 export
must remain disabled until the shared backend advertises the production
capability and reports the whole project export-ready.

## Expected controls

- Lobby is static. `A` enters Garden.
- Garden starts in the clearly labelled LEFT slot.
- `L` selects LEFT and `R` selects RIGHT by changing only the selection marker.
- The numbered `1-2-3-4` animation loops at 400 ms per frame and must not restart
  when the selected slot changes.
- The TIMER indicator fills once, two seconds after entering Garden. Slot changes
  must not restart or delay it.
- `B` returns to Lobby.
- Every fresh Garden entry resets to LEFT, restarts the numbered animation at
  frame 1, hides the timer fill, and starts a fresh two-second timer.

Scene-exit routes have empty action lists. The project contains no audio,
calendar-time logic, parallel regions, remembered scene state, runtime playback
controls or development-encoder workaround.
