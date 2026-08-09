# Font Assets

Default font sheet source:

- `gb_font_default.png`
- `font_manifest.json` (style map for rich text family)

Expected sheet layout for default generator settings:

- 1-bit style source (black glyphs on white background)
- glyph size: `8x8`
- grid: `16 columns x 14 rows`
- first glyph codepoint: `0x20` (space)

Generate firmware font header:

```powershell
python tools/gen_font_assets.py
```

Generated file:

- `Core/Inc/font_assets_autogen.h`

If you need different sheet dimensions or codepoint start:

```powershell
python tools/gen_font_assets.py --glyph-w 8 --glyph-h 8 --cols 16 --rows 14 --first-char 0x20
```

Manifest style slots supported:

- `regular` (required)
- `bold` (optional)
- `italic_lower` (optional)
- `italic_upper` (optional)
- `tiny` (optional)

Rich text parser tags (renderer):

- `{b}` `{/b}`
- `{i}` `{/i}`
- `{t}` `{/t}`
- `{bi}` `{/bi}`
- `{r}` (reset all styles)
