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

Normalized JSON is a compiler intermediate, not an installable package. The
next compiler milestone will emit a bounded `PKG1` PeepPkg container named
with the `.egg` extension.
