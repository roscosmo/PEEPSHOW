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
