# Peep Studio Shape Primitives Handoff

Date: 2026-09-09
Status: implemented on the OS branch; native/host tests and Debug build pass;
hardware visual acceptance pending. The GUI worktree was not edited.

## Why This Changed

`Authoring_pass.peepproj/authoring_pass_test_new.egg` fixes the earlier empty
scenes but uses the GUI's `up_right` line flag. The old OS parser/loader rejected
that flag, and its decoder could only draw the opposite diagonal. The exact
55,932-byte egg with SHA-256
`4710d2583b944ea88b5925c60b968f67e64684855944ad5ae227b31ae1127ef5`
now passes both the shared inspector and native firmware package preflight for
all four scenes. This is not yet a claim that the device has launched it.

## Shared Contract

Editable source uses `kind`; no new asset format or bitmap conversion is needed.

| Source kind | RND2 kind byte | Meaning |
|---|---|---|
| `sprite` | 1 | Existing masked sprite |
| `line` | 2 | Both diagonal directions, arbitrary slope |
| `outline_rect` | 3 | Rectangle outline |
| `filled_rect` | 4 | Solid rectangle |
| `circle` | 5 | Circle outline |
| `ellipse` | 6 | Oval outline |
| `filled_circle` | 7 | Solid circle |
| `filled_ellipse` | 8 | Solid oval |

RND2 uses the existing `RND1` magic with header version `2` and the existing
20-byte element layout. No record-size or container-version change:

- Flags at element byte `6`: `0x01` focus, `0x02` visible, `0x04` up-right line.
- `0x04` is legal only for kind `2`; all other flag bits remain invalid.
- Omitted `line_direction` means `down_right`, encoded without `0x04`.
- `down_right`: `(x,y)` to `(x+width-1,y+height-1)`.
- `up_right`: `(x,y+height-1)` to `(x+width-1,y)`.
- Width and height are positive. A horizontal line has height `1`, a vertical
  line width `1`, and a single-pixel line has both `1`.
- Round shapes require odd width/height, each at least `3`; circle kinds `5/7`
  must be square. Do not silently encode even-sized round shapes.
- All bounds must fit the logical `168x144` canvas. Ink remains black, with
  transparent pixels outside the shape, using existing layers and z-order.
- Filled circles/ovals include their outline and fill its horizontal interior
  spans. There is no independent stroke width or white/clear fill in this slice.
- Non-sprites have no `visual_ref`; package focus remains a visible UI sprite.

Existing RND1 and RND2 records retain their original meaning. Older firmware
does not support the new line flag or kinds `7/8` and must be updated. Internal
firmware element enum IDs differ from wire IDs; the compiler must use the wire
table above, never firmware enum values.

## GUI Work

1. Merge the OS changes into the GUI branch using the normal user-owned git
   workflow. Preserve the GUI branch's later service API version and unrelated
   operations; this OS branch advances its own API baseline from `23` to `24`.
2. Discover `service.hello.state_scene_presentation.element_kinds` and
   `line_directions`. The shared compiler, project validation, package parser,
   and exact preview already implement these capabilities.
3. Keep the existing line tool's `line_direction` behavior. Ensure loading,
   editing, saving, undo/redo, and rebuilding retain it.
4. Expose a fill toggle for circles/ovals, mapping to the separate source kinds
   above. Keep any GUI source validators and drawing previews aligned with the
   shared kind set and odd-dimension constraints. No new firmware draw commands
   or assets are required. This handoff does not add a new set-kind command.
5. Rebuild after edits before exporting. Check the freshly built package with
   the shared inspector rather than exporting a stale build artifact.

Example source element:

```json
{
  "element_id": "solid_oval",
  "kind": "filled_ellipse",
  "x": 28,
  "y": 32,
  "width": 31,
  "height": 19,
  "z_order": 2,
  "layer": "SCENE",
  "visible": true
}
```

## Verification

Host tests exercise deterministic compilation and parsing, all shape decodes,
invalid flags/kinds/geometry, candidate-validation isolation, and pixel parity
using the production C renderer and actual panel-coordinate conversion.
Line tests cover both diagonals across shallow/steep slopes and degenerate
horizontal/vertical/point cases. Round-shape tests include minimum, maximum,
thin/tall, and edge-aligned bounds; filled rows match the outline silhouette.

```powershell
tools/.venv/Scripts/python.exe -m unittest discover -s tools/authoring/tests -q
tools/.venv/Scripts/python.exe tools/authoring/egg_tool.py inspect <freshly-built.egg>
```

Target acceptance still needed: flash this build, install/launch the corrected
authoring-pass egg, inspect its up-right line, and test a GUI-built scene with
outline and filled circles/ovals. Confirm the scene remains composed after
STOP2/wake and that the shell/package recovery paths remain usable.

Related: [[Authoring_Project_Schema_Contract]],
[[Asset_Pipeline_and_Package_Tooling_Contract]],
[[Display_and_Rendering_Contract]],
[[Peep_Studio_Empty_Scene_Validation_Handoff]].
