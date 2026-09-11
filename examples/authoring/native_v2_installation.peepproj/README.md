# Native V2 Installation Fixture

Source-only candidate for OS production validation, USB installation, launch and
cold-boot testing. Derived from the numbered timer fixture's behavior; all prior
passed fixtures remain unchanged. This new source has host verification only.

## Expected Display And Behavior

- One native V2 scene, continuous interaction, two states, eight scene-owned objects.
- Top: digits 1, 2, 3, 4 at (72,12), 24x24, looping at 400 ms/frame.
- Middle: B and A labels beside fixed 24x24 outlines at (28,64) and (116,64).
  A 16x16 marker starts in B at (32,68). A selects the right state, overriding
  only marker X to 120. B returns it to X=32. Repeated presses in the selected
  state have no route, so they cannot re-enter the state or move the marker again.
- Bottom: a fixed 24x24 outline at (72,112). Its 16x16 square at (76,116)
  starts hidden and appears once at 2000 ms through a scene-timer handler.
  It stays visible and fixed through subsequent input and animation.
- At timer expiry the digit is 2, not a restarted 1. A/B never overrides or
  rebinds the animated object. The timer handler has no state destination.
- No audio, scene exits, additional scenes, migration or external image files.
  Digits and labels are build-time system-font assets, not runtime text.

## Host Verification

From `tools/peep-studio`, with `PEEPSHOW_PYTHON` pointing to the authoring Python:

```powershell
node tests/native-hardware-fixture.cjs --admission
```

Checks source validity, continuous interaction, distinct sequential frames, loop
wrap, playback snapshots across A/B, independent-axis overrides, repeated inputs,
hidden timer marker at 1999 ms, one expiry at 2000 ms, no later expiry, and fixed
labels/boxes/revealed marker. Reloaded source is tested through host preview.
Diagnostic host framebuffer and backend build issues go to ignored `dist` output.

Ordinary V2 export/install remains disabled. The backend currently reports
`SCENE_OBJECT_EXECUTABLE_UNAVAILABLE`; that issue is preserved, not suppressed.
No egg is generated. This fixture does not itself establish production admission,
LPBAM resource fit, USB installation, launch or cold-boot success.

After OS advertises production capability, use the unchanged agreed source to
test an actual Studio-built egg through USB install, launch and cold boot. Record
the source commit and resulting OS/firmware commit with that hardware evidence.
