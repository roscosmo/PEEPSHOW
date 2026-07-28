# PeepShow Obsidian Vault

This `docs/` folder is the Obsidian vault root for PeepShow. HW6 is active and HW5 is retained as retired historical material.

The repository root is not the vault. The vault is intentionally isolated from generated firmware, middleware, build output, and CubeMX churn.

Open [[Home]] as the vault landing page.

## Architecture Model

PeepShow is documented as three separated layers:

1. [[Platform_Index|Platform]]
2. [[Engine_API_Index|Engine API]]
3. [[Game_Design_Index|Game Design]] and [[Reference_Game_Index|Reference Game]]

The [[Reference_Game_Index|Reference Game]] is the proof-of-capability game being built for PeepShow. It validates the platform, engine, and tools, but it does not define hardware behavior or platform architecture.

## Core Rule

Game documentation may advance during hardware bring-up.

Game implementation must not drive platform architecture.

The intended flow is:

```text
Reference Game expresses intent.
Engine exposes reusable game-development APIs.
Platform chooses hardware policy.
```

## Vault Domains

- [[Platform_Index|01 Platform]]: authoritative firmware architecture, ownership rules, RTOS, power, storage, display, audio, input, sensors, communication, and validation contracts.
- [[Bring-up_Index|02 Bring-up]]: hardware validation, runbooks, evidence tracking, debug workflows, and phase history.
- [[Engine_API_Index|03 Engine API]]: reusable runtime, package, scene, content, and host-facing contracts.
- [[Game_Design_Index|04 Game Design]]: game-agnostic design method, capability requirements, mechanics research, and design constraints.
- [[Reference_Game_Index|05 Reference Game]]: the actual PeepShow proof game, including lore, creatures, mechanics, encounters, balancing, and game-specific FSMs.
- [[Assets_Pipeline_Index|06 Assets Pipeline]]: asset tooling, package generation, deterministic build rules, and content validation.
- [[Hardware_Index|07 Hardware]]: active HW6 board contracts plus retired HW5 references, pin ownership, clocks, DMA, power rails, wake sources, and CubeMX checks.
- `08 Research`: external research and experimental notes.
- `09 Canvases`: Obsidian canvases.
- `10 References`: ADR process, external references, and retained process documents.
- [[Development_Tooling_Index|11 Development Tools]]: host/dev tooling, CDC developer mode, live-safe Platform tuning, telemetry dashboards, Tracealyzer evidence, orchestration CLI, and digital twin workflows.

## Authority

When documents conflict, resolve the conflict immediately.

For implementation work, priority is:

1. [[Authority_and_Invariants]]
2. relevant Platform or Hardware contract
3. relevant Engine API contract
4. Bring-up evidence and runbooks
5. Game or Reference Game intent notes

Game notes can request capabilities, but cannot redefine ownership, clocks, sleep policy, DMA, storage, or peripheral behavior.
