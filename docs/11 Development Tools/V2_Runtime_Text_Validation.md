# V2 Runtime Text Validation and API 49 Handoff

Status: hardware-validated for the restricted resident V2 export subset.
Service API 49 / export-profile revision 4 enables normal `project.build_package`
export. The target remains development-restricted, not a shipping certification.

## GUI Handoff

- `service.hello.state_scene_presentation.runtime_text` is true; its sibling
  `runtime_text_profile` describes bounds, font, commands and unsupported features.
- The same profile appears at `package_export.v2_profile.runtime_text` and
  `scene_object_authoring.runtime_text`.
- Per-scene `runtime_text` is true only for schema V2, with `runtime_text_profile`.
  Legacy scenes retain false/null. Do not add text to legacy RND2 element controls.
- Use ordinary object commands, undo/redo, save/reload and public build. Continue
  requiring whole-project `export_ready`; existing budgets are unchanged.
- System-font text must stay an object, not be converted to an asset. Existing
  baked assets remain compatible. Custom-font baking is unaffected.
- No dynamic content, substitutions, content overrides or runtime text-edit actions.
- Layer/z-order are accepted in the object definition; this increment does not
  introduce a separate layer/z-order editing command. Position and visibility use
  the advertised defaults/override commands.

## Source and Editing

Scene object `kind: text` adds required `text`, `font_id`, `scale`, `alignment`
alongside normal bounds/layer/z-order/default position/visibility fields.
The only font is `peepshow.system.8x8.basic.v1` (HW4 printable ASCII).
Text contains 1..256 printable ASCII characters or LF. Scale is integer 1..8.
Alignment is `left`, `center` or `right`, applied independently per explicit line;
vertical alignment is top. Blank/trailing lines consume their full line height.
No automatic wrapping, shrinking, glyph substitution or dynamic substitution.
Studio can implement a wrapping textbox by storing explicit LF characters.

`object.add` accepts the definition. `object.set_text` takes `scene_id`,
`object_id`, `text`, `font_id`, `scale`, `alignment`, `width`, `height` and validates
the complete edit. Normal object defaults/overrides control position/visibility;
layer/z-order are stored in the object definition. No state text-content
override or runtime text-edit action is introduced.

## Wire and Ownership

OBJ2 version 1 retains its 20-byte object record and adds kind 9. For this kind:

- Frame field at offset 14 is a STR1 string index, not a sprite index.
- Clip at offset 16 is `0xffff`; reserved at offset 18 is zero.
- Flags bit 0 is visibility, bit 2 retains existing focus metadata. Bit 1 is zero.
- Flags bits 3..5 encode scale minus one; bits 6..7 encode alignment 0/1/2.
  Alignment 3 is invalid. Font identity is implicit in kind 9.
- The UTF-8 string table is unchanged, but runtime text restricts its own spans
  to ASCII/LF and validates their lengths and exact bounds before drawing.

Older firmware rejects the unknown kind rather than misinterpreting it.
No text sprite assets or new chunk are generated. Internal render element 13
uses asset_id as the string index and style_id as flags shifted right by three.
Runtime publications remain pointer-free; the catalog carries immutable STR1
bytes under the existing package lease. Private copies rebase the string span.

## Hardware Fixture

Generate with:

```powershell
python tools/authoring/build_runtime_text_fixture.py --egg-output firmware/peepshow_hw6_fw0/build/runtime_text.egg
```

Flash the new normal Debug firmware, then transfer/install this egg through the
existing MSC installation flow. The old firmware does not support kind 9.
The development encoder reproduces the original bench artifact. API 49 public
export produces identical bytes; Studio must use the public operation.

Expected screen:

- `Text 2x` upper-left and a four-phase moving square upper-right (250 ms/frame).
- Two centered 2x lines `abc XYZ` and `012 !?`.
- `Aa` starts left; `3x` remains centered; `Visible` is right-aligned below.
- A toggles `Aa` between left/right and hides/restores `Visible` once per press.
- B restores the initial state. Animation phase must continue across A/B.
- HOLD START opens shell; resume restores the scene. Reboot starts fresh.
- No ghost glyphs, clipped glyphs, shifted static labels or implicit scale changes.
- After release, autonomous animation must return normally.

After observation, wake normally before halting and use the existing
`__fw0_object_installed_prints.gdb`. Require successful admission/render,
four steps at 250 ms, fault-free wake reconciliation and visible continuity.
Counters do not measure low-power current.

## Host Evidence

2026-09-18 HW6 observation: installed source/model/bytes = 3/2/2052, generation 24;
admission token/complete/lease/status = 13/13/0/0; display request/complete/result =
10/10/0; LPBAM steps/quantum = 4/250; chunks/bytes = 7/4088; measured/reconciled =
8/8. User confirmed visible text, A moving Aa and toggling Visible, and continuous
animation through the changes. This is functional evidence, not a current measurement
or exhaustive device coverage of every glyph/scale/alignment.

Public build SHA-256 matches the hardware artifact:
`14e05d8e70e8478561af202a05f102d81946ac23d7192028254fd708b67eb83d`.
API 49 tests cover public creation/editing, undo/redo, save/reload, rejection
without mutation, export readiness, unbaked text-only packages and legacy gating.

Tests compare real C package decode/projection/raster output with preview,
all integer scales and alignments, multiline text, state move/hide, and text-only
packages without assets. Malformed references/styles/bounds are rejected by C
and Python. Candidate admission checks text-bearing LPBAM payloads without
mutating live display/package state; private-copy catalog rebasing is tested.
All 95 font glyphs additionally match HW4 and the frozen host font bytes.
