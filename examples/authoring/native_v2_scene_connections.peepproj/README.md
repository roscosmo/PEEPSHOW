# Native V2 Scene Connections

Source-only API 43 host fixture. Open this folder in Studio.

- Lobby: A enters Garden.
- Garden: B returns to Lobby.
- Both scenes have clearly rendered names, button hints and a border.
- Each scene has one default state and one named exit with an empty-action route.
- Garden's return uses an editor-only Go To Lobby alias in Scene Flow.
- Re-entry is fresh, not remembered selection or retained scene resume.
- No audio, timers, memory, parallel regions or private animation records.

Host verification passed three complete A/B round trips, default-state entry and
distinct rendered framebuffers. Multi-scene Build/Export remains blocked by
backend build issues. This is not hardware evidence; no egg was generated.

From `tools/peep-studio`, with `PEEPSHOW_PYTHON` configured:

```powershell
node tests/native-scene-connections-fixture.cjs
```

This verifies without saving source changes. `--create` is initial generation
only and refuses to overwrite an existing fixture. The Electron regression
`tests/scene-connections-workflow.cjs` edits a disposable copy, not this source.
The fixture's handoff commit is the user-created commit containing this folder.
