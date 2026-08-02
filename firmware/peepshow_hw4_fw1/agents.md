# AGENTS.md

These instructions define how agents must operate when contributing to
PeepShow V5 (ThreadX). These rules are mandatory.

If code conflicts with `/docs/*.md`, the documentation is authoritative
unless explicitly overridden by the user.

---

# TOP PRIORITY: PROJECT UNDERSTANDING BOOTSTRAP (MANDATORY)

Before proposing ANY firmware code changes, the agent must first perform a
minimum project understanding pass.

## REQUIRED READING ORDER (minimum set)
1. `docs/authority.md` (cross-cutting invariants; wins over all)
2. `README.md` (doc index + mode overview)
3. The single domain doc relevant to the work:
   - Power/STOP2: `docs/power_management.md`
   - ThreadX ownership/queues: `docs/rtos_architecture.md`
   - Display/rendering: `docs/display_and_rendering.md`
   - Storage/install/update: `docs/storage_and_updates.md`
   - Asset generation: `docs/asset_pipeline.md`
   - Audio pipeline: `docs/audio.md`
   - Peripheral recovery: `docs/peripheral_robustness.md`
   - FSM rules: `docs/state_machine.md`
   - Tiled/maps: `docs/Tiled_map_integration.md`
   - Debug rules: `docs/debugging.md`
   - Knobs system: `docs/knobs.md`

## REQUIRED OUTPUT AFTER READING (short, no PLAN)
Before any PLAN/SUMMARY gate (see below), the agent must state:

- **Domain authority:** which doc(s) govern this change (exact filenames)
- **Hard invariants touched:** list the relevant invariants from `docs/authority.md`
- **Ownership impact:** name the owning thread(s) involved, if any
- **Mode impact:** which mode(s) are affected (STOP/STATIC/REALTIME/FLASHING)

If the agent cannot confidently name the governing docs or invariants:
**stop and request clarification or patch docs first** (do not guess in code).

---

# TOP PRIORITY: PLAN/SUMMARY GATE (ONLY BEFORE MAKING CHANGES)

The PLAN + SUMMARY process is required ONLY when you are about to:

- write or modify firmware code
- change existing source files
- generate patches/diffs
- implement a feature or fix

It is NOT required when you are:

- reading files (agents.md, README, logs, code)
- answering questions
- explaining concepts
- investigating or diagnosing
- asking clarifying questions
- summarizing findings
- editing documentation only (unless explicitly requested to gate docs)

---

## RULE: WHEN TO USE PLAN + SUMMARY

### Use PLAN + SUMMARY only if:
You will modify firmware code next.

### Do NOT use PLAN + SUMMARY if:
You are only reading, discussing, or analyzing.

Do not produce PLAN unless code modification is imminent.

---

## REQUIRED FORMAT (ONLY BEFORE EDITING CODE)

When changes are actually needed:

### PLAN
- Goal:
- Files to touch:
- Steps:
- Risks:
- Test plan:

### SUMMARY FOR APPROVAL
- What will change:
- Why:
- How we verify:
- **Docs referenced (must include authority.md):**
- **Invariants checklist (see below):**

END SUMMARY — WAIT FOR "GO"

No code before approval.

---

## INVARIANTS CHECKLIST (MUST APPEAR IN SUMMARY)

The SUMMARY must explicitly confirm each item below that applies:

- [ ] Single-owner peripheral model preserved (no cross-thread HAL access)
- [ ] No dynamic allocation introduced
- [ ] No runtime FAT/FileX access introduced in REALTIME
- [ ] No unbounded loops / no hidden retries
- [ ] ISR work remains minimal; defers via ThreadX objects
- [ ] STOP2 entry/exit quiesce/resume rules preserved (if touched)
- [ ] Clock/timebase correctness preserved (SysTick / ThreadX tick) (if touched)
- [ ] Display SPI invariants preserved (LSB-first, CS active-high, etc.) (if touched)
- [ ] Audio remains DMA-buffered and bounded CPU (if touched)
- [ ] Knob pipeline used for tunables (if new constants introduced)

If any checkbox cannot be honestly checked, the agent must state why and
what mitigation/test covers it.

---

## EXCEPTION: BUILD ERRORS

If the user provides a compiler/build/runtime error:

- You may immediately propose and apply the minimal fix.
- Only modify files strictly required to resolve the error.
- Do not refactor surrounding architecture.
- Do not introduce new features.

---

## IMPORTANT BEHAVIOR RULES

- Do not refactor architecture unless explicitly requested.
- Do not rename threads, queues, or subsystems unless asked.
- Do not introduce optional features or “nice-to-have” changes.
- Do not silently change behavior.
- All behavior changes must be declared in SUMMARY.
- If architectural impact is unclear, ask before proceeding.
- When replacing a function, provide the complete function body.
- Do not provide partial snippets that require searching for insertion points.

---

# Hard Rules (Do Not Violate)

- Do not modify CubeMX-generated code outside `/* USER CODE BEGIN */` blocks:
  - `Core/Src/main.c`
  - `Core/Src/app_threadx.c`
  - `Core/Src/stm32u5xx_it.c`
  - `Core/Src/stm32u5xx_hal_msp.c`
  - Anything under `Drivers/`, `Middlewares/`, or `cmake/stm32cubemx/`

- Do not introduce dynamic memory or custom heap systems.
  - ThreadX objects must be created deterministically at init.
  - No hidden `malloc()` usage.

- Do not introduce non-deterministic behavior.
  - No hidden retries.
  - No random timing jitter.
  - No uncontrolled background loops.

- Do not add blocking delays inside ISRs.
  - ISRs must remain minimal and deterministic.
  - Defer work via ThreadX mechanisms only:
    - `TX_QUEUE`
    - `TX_EVENT_FLAGS_GROUP`
    - `TX_SEMAPHORE`
    - thread notifications

- Blocking inside threads must be bounded and justified.
  - No polling loops.
  - No infinite waits without timeout.

- Do not access peripherals outside their owning thread.

- Do not refactor working drivers unless explicitly requested.
  If it compiles and behaves as intended, leave it alone.

- Do not change clock tree, power modes, linker scripts, startup files,
  or CubeMX middleware configuration unless explicitly requested.

- Do not introduce runtime FAT access for gameplay or audio.
  FileX is transport only.

- Do not stream assets from FAT during runtime.

## Tooling / Build Rules (Mandatory)

- Agents must **NOT** run `cmake --build`, `ninja`, Cube build steps, or any full project build.
  - Reason: builds may hang/churn indefinitely in this environment and waste time.
- Agents must **NOT** run long-running commands (format-all, full grep over huge trees, mass regeneration)
  unless explicitly requested by the user.

Allowed alternatives:
- Reason about compile errors from logs the user provides.
- Propose minimal patches with clear file/line context.
- If verification is needed, the agent must provide an explicit manual command for the user to run locally,
  and state what output to look for.



If unsure, ask first.

---

# Code Style

- Prefer `static` functions and file-scope state.
- Use explicit types (`uint32_t`, `int16_t`, etc.).
- Avoid plain `int`.
- No recursion.
- Minimal macros.
- Prefer `static const` tables for configuration.
- ISR code must post to RTOS objects and return immediately.
- RTOS objects are created once at init (`MX_ThreadX_Init` / `App_ThreadX_Init`).

---

# KNOBS RULE (Compile-Time Tuning System)

All user-tunable firmware values must flow through the knobs system.

Pipeline:

- `config/knobs.json` (single source of truth)
- `tools/gen_knobs.py`
- `Core/Inc/knobs_autogen.h`

Do NOT scatter tuning constants across source files.

---

## When to Add a New Knob

If a new value affects:

- gameplay feel
- UI timing
- RTOS scheduling
- power/performance tradeoffs
- debug toggles
- hardware polling rates
- hardware control values

… it must be added as a knob.

Before adding a hardcoded value, ask:

Is this something the user may want to tune?

Only apply this rule to firmware-visible constants,
not private helper locals.

---

## Knob Hygiene Rules

- `config/knobs.json` is the only source of truth.
- Never manually edit `knobs_autogen.h`.
- Add a schema entry in `config/knobs.schema.json` with description and constraints.
- Regenerate the header after modification.

Knob definitions must remain clean and organized:

- Group related knobs logically (gameplay, UI, power, debug, etc.).
- Use clear, descriptive names.
- Avoid redundant or overlapping knobs.
- Do not leave unused or deprecated knobs in the file.
- Keep formatting consistent and readable.

`knobs.json` is a configuration interface, not a dumping ground.

---

# DEBUGGING RULES

PeepShow debugging is hardware-first.

Supported interfaces:

- SWD via ST-Link V3 MINI-E
- SWO (preferred runtime visibility)
- USB CDC (optional, timing-sensitive)

UART logging is not supported.

---

## debug.gdb Contract

The root `debug.gdb` file is authoritative.

Rules:

- Use `debug.gdb` for breakpoint-based debugging.
- Do not invent arbitrary breakpoint locations.
- Maximum 5 breakpoints total.
- Breakpoints must not be placed in:
  - high-frequency ISRs
  - DMA callbacks
  - display flush loops
  - audio refill loops
  - frame loop hot paths

Prefer SWO event markers over breakpoints.

---

## HardFault Handling

HardFault capture is mandatory.

Always extract:

- PC / LR
- stacked registers
- CFSR / HFSR
- MMFAR / BFAR (if valid)

Never treat HardFaults as “random”.

---

## SWO Logging Rules

- Structured event codes only.
- No continuous streaming.
- Rate-limited.
- Non-blocking.

---

## STOP2 Debug Discipline

- Enable debug-in-low-power when investigating STOP2.
- Prefer SWO over breakpoints around STOP transitions.
- Do not guess about wake causes; request evidence.

---

## Agent Debug Behavior

When diagnosing runtime issues:

1. Request SWO markers or HardFault record.
2. Refer to `debug.gdb` for strategic breakpoints.
3. Avoid speculative fixes.
4. Ask for trace data before proposing architectural changes.

---

These rules are mandatory.
Violations cause instability, non-determinism, or architectural drift.
