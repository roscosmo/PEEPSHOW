# Peep Studio

Peep Studio is the desktop shell for authoring PeepShow projects and exporting
`.egg` packages. The Electron application is intentionally a client of the
existing Python authoring service; it does not duplicate package validation,
preview execution, or compilation rules in TypeScript.

## Current shell

- opens a `.peepproj` directory or the checked-in STATE example;
- lists authored scenes and starts a selected-scene preview directly;
- renders the exact 168x144 1bpp framebuffer returned by Python;
- injects logical A/B/L/R input and advances deterministic preview time;
- shows runtime state, variables, validation issues, and package facts;
- builds and exports the authoritative `.egg` bytes produced by Python.

Node editing, asset import, project mutation, device installation, SEQUENCE,
and PROGRAM authoring are deliberately outside this first shell.

## Run

From `tools/peep-studio`:

```powershell
npm install
npm run dev
```

Use `PEEPSHOW_PYTHON` to select a Python executable when `python` is not the
correct command. The sidecar runs:

```powershell
python -u tools/authoring/egg_tool.py service
```

The Python process uses protocol version 1 and is owned by the Electron main
process. The React renderer has no Node.js access.

## Ownership

- `electron/main.ts`: desktop lifecycle, project/save dialogs, Python process,
  and filesystem export.
- `electron/preload.ts`: narrow context-isolated IPC bridge.
- `src/`: presentation and user interaction only.
- `tools/authoring/peepshow_authoring/`: all semantic authority.

See
`docs/11 Development Tools/Peep_Studio_PeepOS_Link_Contract.md` before changing
the editor-to-OS boundary.
