# Pet UI Assets

Place pet-mode PNG assets here for generation by `tools/gen_pet_sprites.py`.

Manifest:

- `pet_assets.json`
- `pet_sheet.json`
- `pet_assets.sheet_template.json` (example for 2x5 icon sheet layout)

Expected files (16x16 RGBA PNG, referenced by manifest):

- `icons/feed.png`
- `icons/play.png`
- `icons/start.png`
- `icons/options.png`
- `states/sleep.png`
- `states/idle.png`
- `states/feed.png`
- `states/play.png`
- `states/rest.png`

Generation command:

```powershell
python tools/gen_pet_sprites.py
```

Output header:

- `Core/Inc/ui/ui_pet_assets_autogen.h`

The generator reads icon/state IDs, row order, and pet-state mapping from
`pet_assets.json` and reads animation frame/clip data from `pet_sheet.json`.
For menu icons, `pet_assets.json` supports either:

- `icons` (individual PNGs), or
- `icon_sheet` (single sheet with per-icon cell coordinates)

Optional selection highlight background:

- `selection_bg` (single 16x16 PNG path), or
- `icon_sheet.selection_bg` (cell coordinate in icon sheet)

`pet_sheet.json` points at the packed sprite sheet PNG and defines:

- frame names -> sheet cell coordinates
- animation clips (frame sequence, `frame_ms`, loop, tick domain)
- pet-state -> clip mapping

Clip timing fields:

- `frame_ms` (preferred): scalar (applies to all frames) or per-frame array.
- `dur_ticks` (legacy): accepted for migration; converted by domain.

Supported tick domains:

- `active`
- `stop_wake_1hz` (legacy alias: `rtc_1hz`)
- `stop_select_2hz`

If files are missing, the generator emits fallback placeholder glyphs so pet mode
remains functional.
