# PeepShow Authoring Tools

This directory contains the host-side authoring model and compiler pipeline.

The first implemented subset supports editable `.peepproj` directories with
one or more `STATE_SCENE` source files. It validates stable IDs, bounded scene
tables, symbolic input routes, retained render elements, reactive waiting
visuals, and interaction policy.

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
python tools/authoring/egg_tool.py embed examples/authoring/state_slice.peepproj --output firmware/peepshow_hw6_fw0/Core/Src/ps_embedded_egg_autogen.c
```

Normalized JSON is a compiler intermediate, not an installable package. The
`build` command emits the bounded deterministic `PKG1` PeepPkg container using
the `.egg` extension. `inspect` reparses the package, validates all container
bounds, CRCs, SHA-256 integrity, and supported STATE chunk records, then prints
a semantic summary. `embed` uses the same compiler and emits a generated array;
it is not a second package format or a hand-maintained firmware descriptor.

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

`project.load` starts a new monotonically increasing project revision. Every
project operation must supply that revision; stale requests are rejected rather
than being applied to a newer document.

The build operation returns the exact existing compiler output as base64 package
bytes, package metadata, and the matching deterministic compatibility report.
It does not choose a destination or write an installable file. The V1 report
marks the current HW6 development profile as `pending_validation` and
`dev_only`; it does not claim shipping authority before target-profile closure.

Project editing, save, undo/redo, and preview operations are intentionally not
advertised yet. Their semantics belong to later authoring milestones.

Run the focused host tests with:

```powershell
python -m unittest discover -s tools/authoring/tests -p "test_*.py"
```
