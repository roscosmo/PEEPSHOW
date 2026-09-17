# Native V2 Lobby/Garden Audio Fixture

Source-only Studio and OS integration fixture for the API 45 resident sampled-SFX
profile. This is a separate derivative of the passed audio-free Lobby/Garden
fixture; the original source remains unchanged.

## Expected controls

- Lobby is static. `A` enters Garden with no scene-exit action.
- Garden starts in the clearly labelled LEFT slot.
- `L` selects LEFT and `R` selects RIGHT. A matched change moves only the
  selection marker and plays the short Garden selection cue.
- Repeated `L` while already LEFT and repeated `R` while already RIGHT do
  nothing and do not play a cue.
- The numbered `1-2-3-4` animation loops at 400 ms per frame and must not restart
  when the selected slot changes.
- Two seconds after fresh Garden entry, TIMER fills once and its targetless
  handler starts the six-second Garden timer effect. Slot changes
  do not restart or delay the timer.
- `B` returns to Lobby with an empty scene-exit action list. Entering Lobby
  during the long tone should allow the already-active cue to continue.
- Every fresh Garden entry resets to LEFT, restarts the numbered animation at
  frame 1, hides the timer fill, and starts a fresh two-second timer.
- Holding START during playback enters the shell and discards active package
  SFX. Returning to the package remains silent until a new local cue request.

The project contains no music, looping audio, calendar-time logic, remembered
scene state, parallel regions, runtime playback controls or development encoder.

## Audio sources

- `garden_select.wav` is prepared from `assets/audio/UI_Hover.wav`, trimmed to
  its audible range and normalized to `-6 dBFS` through Studio's audio preparation path.
- `garden_timer_tone.wav` is prepared from `assets/audio/SFX_menuloop.wav`,
  trimmed to six seconds and normalized to `-6 dBFS` through the same path.

The project-local WAV files are deliberate prepared copies. The shared source
library remains unchanged and is not referenced outside the project at runtime.
