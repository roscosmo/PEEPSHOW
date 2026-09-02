# PeepShow Authoring Tools

This directory contains the host-side authoring model and compiler pipeline.

The first implemented subset supports editable `.peepproj` directories with
one or more `STATE_SCENE` source files. It validates stable IDs, bounded scene
tables, symbolic input routes, retained render elements, reactive waiting
visuals, and interaction policy. Projects may also declare masked 1bpp PNG
asset catalogs. Those sources compile into deterministic asset-table,
sprite-bank, and animation-table chunks; the independent package reader
validates and reconstructs the exact logical pixels and masks.

Install the pinned host dependency before using PNG asset catalogs:

```powershell
python -m pip install -r tools/authoring/requirements.txt
```

The initial PNG importer accepts exact black and white visible pixels only.
Alpha zero is transparent and every nonzero alpha value is opaque. Tone
reduction and dithering are intentionally deferred.

Add one or more catalogs to `project.json`:

```json
"asset_sources": ["assets/catalog.json"]
```

The initial catalog shape is:

```json
{
  "schema_id": "peepshow.authoring.assets",
  "schema_version": 1,
  "assets": [{
    "asset_id": "cursor",
    "asset_type": "masked_1bpp",
    "source_path": "assets/cursor.png",
    "source_format": "png",
    "frames": [{
      "frame_id": "cursor.phase_a",
      "source_rect": {"x": 0, "y": 0, "width": 8, "height": 16},
      "pivot_x": 0,
      "pivot_y": 0
    }]
  }],
  "animations": [{
    "animation_id": "cursor.blink",
    "frame_refs": ["cursor.phase_a"],
    "frame_duration_ms": [250],
    "loop_policy": "loop"
  }]
}
```

Asset and catalog paths are project-relative and may not escape the
`.peepproj` directory.

Validate the example project:

```powershell
python tools/authoring/egg_tool.py validate examples/authoring/state_slice.peepproj
```

Write deterministic normalized authoring data:

```powershell
python tools/authoring/egg_tool.py normalize examples/authoring/state_slice.peepproj --output build/state_slice.normalized.json
```

Build and independently validate an installable package:

```powershell
python tools/authoring/egg_tool.py build examples/authoring/state_slice.peepproj --output build/state_slice.egg
python tools/authoring/egg_tool.py inspect build/state_slice.egg
```

Generate the exact same package as a firmware-resident C byte array for the
pre-installation vertical slice:

```powershell
python tools/authoring/egg_tool.py embed examples/authoring/state_transition_slice.peepproj --output firmware/peepshow_hw6_fw0/Core/Src/ps_embedded_egg_autogen.c
```

Normalized JSON is a compiler intermediate, not an installable package. The
`build` command emits the bounded deterministic `PKG1` PeepPkg container using
the `.egg` extension. `inspect` reparses the package, validates all container
bounds, CRCs, SHA-256 integrity, and supported STATE chunk records, then prints
a semantic summary. `embed` uses the same compiler and emits a generated array;
it is not a second package format or a hand-maintained firmware descriptor.

The current STATE logical-source set is `BUTTON_A`, `BUTTON_B`, `BUTTON_L`,
`BUTTON_R`, `BUTTON_START`, `JOY_LEFT`, `JOY_RIGHT`, `JOY_UP`, `JOY_DOWN`,
`JOY_UP_LEFT`, `JOY_UP_RIGHT`, `JOY_DOWN_LEFT`, and `JOY_DOWN_RIGHT`. Bindings
may select `press`, `release`, `hold`, or `repeat`; omitted event kind means
`press`. Each STATE scene selects `four_way` or `eight_way` joystick policy.
`BUTTON_START` supports package `press` only because PeepOS owns its long
gesture. Normalized joystick vectors remain deferred to PROGRAM scenes.

## Authoring Service

The desktop editor boundary is a long-running, single-session Python service
using versioned newline-delimited JSON over standard input/output:

```powershell
python tools/authoring/egg_tool.py service
```

Each request has exactly four fields:

```json
{"protocol_version":1,"id":"request-1","operation":"service.hello","params":{}}
```

Implemented operations:

- `service.hello`
- `service.shutdown`
- `project.load`
- `project.validate`
- `project.normalize`
- `project.build_package`
- `project.compatibility_report`
- `project.apply_commands`
- `project.save`
- `project.undo`
- `project.redo`
- `project.scene_thumbnails`
- `project.audio_audition`
- `project.preview_reset`
- `project.preview_input`
- `project.preview_advance`

`project.load` starts a new monotonically increasing project revision. Every
project operation must supply that revision; stale requests are rejected rather
than being applied to a newer document.

The build operation returns the exact existing compiler output as base64 package
bytes, package metadata, and the matching deterministic compatibility report.
It does not choose a destination or write an installable file. The V1 report
marks the current HW6 development profile as `pending_validation` and
`dev_only`; it does not claim shipping authority before target-profile closure.

Service API version 21 provides deterministic selected-STATE-scene preview,
direct STATE-to-STATE replacement, and a `state_scene_presentation` capability
block in `service.hello`. A
reset names the scene to launch directly, an input operation supplies one
logical source plus an optional lifecycle event kind, and an advance operation supplies explicit elapsed
milliseconds. Every response contains the current compiled state, timeline,
variables, and an exact `168 x 144` packed 1bpp framebuffer. Preview never
reads source assets after reset: it builds and independently parses the `.egg`,
then executes those validated package records.

STATE routes declare exactly one `target_state` or `target_scene`.
`route.set_target` accepts the same exclusive fields. A scene target must be an
existing STATE scene and may carry only package-global `play_sfx` actions;
scene-local variable, render, element, and system actions remain invalid on a
direct scene replacement. Preview emits the cue event, enters the destination
entry state, resets destination-local variables, and starts the destination
timeline at its settled step.

The exact preview accepts `RND2` package-backed masked 1bpp sprites plus line,
outline rectangle, filled rectangle, circle, and ellipse elements. Package
content may use `BACKGROUND`, `SCENE`, and `UI`; `OVERLAY` is system-owned.
Route actions may atomically show/hide a destination element, move it within
the 168x144 panel, or select a same-sized retained sprite frame. Visibility and
position flow into linked waiting visuals; frame selection does not replace an
authored waiting animation. Service API 17 also allows a route to select one
compatible bounded waiting-element track with explicit preserve/rebase policy.
Runtime text and runtime scaling remain unavailable.

Service API 18 adds host/package support for one-shot STATE sampled SFX. Asset
catalogs can import PCM WAV sources and define symbolic cues; local STATE route
actions can emit `play_sfx`. Package compilation produces optional `AUD1`,
`ADB1`, and `ACU1` chunks containing mono 16 kHz 4-bit IMA ADPCM in fixed
256-sample blocks. Preview reports emitted cue events and
`project.audio_audition` returns a WAV decoded from the exact packaged bytes.
HW6 target loading and `thAudio` playback are brought up for one package-backed
streamed STATE sampled-SFX voice, including SFX that survive a scene replacement
inside the active package. Package suspension, exit, replacement, and unmount
remain audio ownership boundaries.
The newline-delimited JSON protocol remains version 1, and loaders retain
`RND1` compatibility.

Project editing, save, and undo/redo operations are available through the
versioned service boundary; use `service.hello` for the authoritative operation
list.

Run the focused host tests with:

```powershell
python -m unittest discover -s tools/authoring/tests -p "test_*.py"
```
