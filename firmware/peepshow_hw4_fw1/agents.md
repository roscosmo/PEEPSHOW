These instructions define how agents must operate when contributing to
PeepShow V5 (ThreadX). These rules are mandatory.

If code conflicts with `/docs/*.md`, the documentation is authoritative
unless explicitly overridden by the user.

---

# TOP PRIORITY: COMMUNICATION QUALITY (MANDATORY)

The agent must optimize for user understanding first, not template completion.

## DEFAULT RESPONSE SHAPE

Unless the user explicitly asks for full detail, use this order:

1. **Conclusion first**
   - 1-3 short sentences max.
   - State what is true, false, broken, or next.
   - No jargon unless needed.

2. **Why**
   - Short explanation in plain English.
   - Explain what matters in real behavior, not process terms.

3. **Evidence**
   - Only the most relevant evidence.
   - Do not dump raw command output unless the user asked for it.
   - Translate debugger/helper output into meaning.

4. **Next step**
   - Give the exact next action.
   - Keep it short and executable.

Do not front-load answers with large templates when a short answer would do.

## CONCLUSION-FIRST RULE

For questions, approvals, validations, and result interpretation,
the first line must answer the user's actual concern directly.

Examples:
- "Yes, that result means the patch is working."
- "No, that test did not measure FLASHING."
- "That command was wrong for this phase."
- "Those non-zero thread counts do not mean rendering happened."

Do not begin with headers unless the user asked for a formal report.

## PLAIN ENGLISH OVERRIDES TEMPLATE

If a required format conflicts with clarity, clarity wins.

This means:
- A required gate may still be used, but it must be brief.
- Do not dump long PLAN/SUMMARY boilerplate before explaining the situation simply.
- Do not hide the real point inside structure.

Bad:
"SUMMARY FOR APPROVAL ... invariants checklist ..."

Good:
"Plain English: FLASHING still allowed normal storage requests. I’m going to block those and keep FileX unmounted while FLASHING is active. Then I’ll verify STATIC still resumes normally."

## DO NOT MAKE THE USER TRANSLATE

Do not make the user translate process jargon into real meaning.

Forbidden without immediate translation:
- control-plane
- isolation path
- arbitration
- workflow
- gate
- validation
- state transition
- ownership model
- suppression proof
- active work executes
- no-op
- metric
- signal
- path

If such terms are used, the same sentence must explain them in normal words.

Example:
Bad:
"Need explicit render/gameplay suppression proof in FLASHING."

Good:
"We still need to prove that, in FLASHING mode, the game is not actually drawing frames or running gameplay logic."

## SHORT APPROVAL GATES

When waiting for approval, keep it tight.

Default maximum:
- 5 lines for a simple patch
- 10 lines for a risky patch

Do not paste full checklist blocks unless the user asks for full detail.

Default approval format:

### BEFORE I CHANGE CODE
- Problem:
- Fix:
- Check:
- Risk:
- Wait for GO.

If the user says "in English", collapse further to 3 lines max.

## OWN ERRORS CLEANLY

If the agent gives a wrong instruction, wrong helper name, wrong assumption,
or wrong phase guidance, it must say:

1. what was wrong
2. why it was wrong
3. the corrected instruction

Do not soften it with filler.
Do not act as if the user misunderstood.

Example:
"I gave the wrong test step. Mounting FileX here was incorrect because MSC is still the last phase. Use ps_mode_flashing + storage probes instead."

## HELPER / COMMAND DISCIPLINE

Do not guess debugger helpers, command names, or probe aliases.

If referring to a helper:
- prefer helpers already used in the current conversation
- if unsure, say it is unsure
- do not invent command names

If a helper was wrong:
- acknowledge it directly
- give the corrected sequence in one block
- do not bury the correction in explanation

## RESULT INTERPRETATION RULE

The agent must distinguish between:
- thread got scheduled
- real subsystem work happened

Explain this plainly.

Example:
"That counter only shows the thread woke up. It does not prove the game drew anything."

Do not assume the user will infer this from raw metrics.

## DO NOT OVER-CEREMONIALIZE

Do not respond like a process auditor when the user needs a working technical partner.

Avoid:
- repeated gate language
- repeated references to compliance
- repeated checklist restatements
- repeated doc-name recitals unless relevant
- verbose restatement of already accepted context

Once context is established, use it.

## WHEN THE USER IS FRUSTRATED

If the user is irritated, swearing, or clearly impatient:
- shorten immediately
- answer the core issue first
- cut headings unless necessary
- do not become more formal
- do not repeat previous structure
- do not defend the earlier wording

The correct response style is:
direct, calm, concrete, brief

## PREFERRED STYLE EXAMPLES

Bad:
"Current code allows non-USB activity overlap and competing storage access in FLASHING, which violates HG-3 and authority docs."

Better:
"Right now FLASHING mode is still letting normal storage work through. That is bad because USB flashing is supposed to own that area by itself."

Bad:
"Need explicit render/gameplay suppression proof in FLASHING."

Better:
"We still need to prove the game is not actually drawing or running while FLASHING is active."

Bad:
"ps_delta_runs is the wrong metric."

Better:
"That counter only tells us the thread woke up. It does not tell us the thread actually rendered a frame."

Bad:
"Suggested validation on target..."

Better:
"Run these three commands. This will tell us whether FLASHING is really blocking normal storage work."

---

# TOP PRIORITY: DIAGNOSIS GATE (MANDATORY FOR BUGS / RUNTIME ISSUES)

When investigating a malfunction, regression, persistence issue, or
unexpected runtime behavior, the agent MUST produce a **DIAGNOSIS**
before proposing any fix.

The agent is NOT allowed to jump directly from investigation to code changes.

For bug/debugging work, explain the issue in plain English first,
then provide the technical evidence.

## REQUIRED DIAGNOSIS OUTPUT

Before proposing a fix, produce a section titled:

### DIAGNOSIS

It must include:

**PLAIN ENGLISH**
- What is happening:
- Why it matters:
- Why the current evidence is trustworthy or not trustworthy:
- What we need to do next:

**Observed behavior**
- What the user reported or what the logs show.

**Expected behavior**
- What the system should be doing.

**Evidence**
- Exact variables, functions, code paths, debugger observations, or runtime data
  observed during investigation.

**Root cause hypothesis**
- The specific mechanism causing the bug.
- Must reference actual code, actual data flow, or actual runtime evidence.

**Confidence level**
- HIGH / MEDIUM / LOW.

The diagnosis must be understandable by a human reading it for the first time.

Root cause claims must reference specific files, functions, variables,
or runtime data observed during investigation.

## RULE: NO FIX BEFORE DIAGNOSIS

If the issue is a bug, regression, persistence failure, or incorrect behavior:

The agent must STOP after DIAGNOSIS and wait for confirmation.

Do NOT produce code changes yet.

The user must confirm:
- "Diagnosis correct"
or
- request more investigation.

Only then may the agent propose a code change.

## WHEN DIAGNOSIS IS REQUIRED

Diagnosis is required when:
- debugging runtime behavior
- persistence issues
- incorrect state
- race conditions
- sensor/input anomalies
- unexpected resets
- load/save issues
- hardware interaction problems

Diagnosis is NOT required for:
- implementing new features
- refactors requested by the user
- compile errors
- documentation edits

## REQUIRED STYLE FOR DEBUGGING EXPLANATIONS

For debugging or runtime investigation, present findings in this order:

### PLAIN ENGLISH
- What this means:
- Why the current result is or is not trustworthy:
- What we need to do next:

### TECHNICAL EVIDENCE
- Relevant files / functions / variables / debugger commands:
- Exact evidence supporting the conclusion:

## FORBIDDEN BEHAVIOR

Do NOT open with:
- raw debugger command output without explanation
- internal helper function names without context
- acronym-heavy or shorthand-heavy explanations
- approval requests before the issue has been explained plainly

Do NOT treat debugger visibility as equivalent to trustworthy runtime behavior
if the debugger itself alters low-power or timing behavior.

If the measurement method changes system behavior, the agent must explicitly say so.

---

# TOP PRIORITY: CLI MINIMISATION + CACHE DISCIPLINE (MANDATORY)

Goal: minimise CLI/tool calls, especially repeated reads of unchanged docs/files,
while still preventing stale-version edits.

## Session Cache Contract (Mandatory)

- Treat any file read in THIS conversation as cached.
- Do NOT re-read the same unchanged file repeatedly “just to be safe”.
- Operate from a cached mental summary until the cache is invalidated.

## Version Stamp Tracking (Mandatory)

When reading a file for the first time in the conversation, capture a simple
version stamp and keep it internally alongside the cached summary:

- Preferred: git `HEAD` commit (or file-level commit if easy)
- Otherwise: file `mtime + size`

You do NOT need to print the stamp unless relevant, but you MUST track it.

## Cache Invalidation Rules (Mandatory)

A cached file must be treated as stale if ANY occur:
- The user says or implies the file was edited/updated.
- The user pastes new content that supersedes what was cached.
- The agent switches branches/commits (or user references a different branch/commit).
- A build/runtime error references line numbers/symbols that don’t match cached content.
- The agent is about to edit a file and has not verified it is unchanged since last read.

## Pre-Edit Freshness Check (Mandatory)

Before editing ANY file that was cached earlier in the conversation:
- Verify it is unchanged vs its cached version stamp.
- If you cannot verify, do a SINGLE targeted re-open of that file only.
- No repo-wide scans just for reassurance.

## Default CLI Restrictions (Mandatory)

- No orientation commands by default: avoid `ls`, `tree`, broad `find`.
- No broad repo searches by default: avoid whole-repo `rg/grep`.
- If CLI is necessary:
  - keep scope tight (single dir / single file)
  - cap output to the relevant section
  - state the reason in ONE sentence

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

## Bootstrap Read-Once Rule (Mandatory)

- Perform the REQUIRED READING ORDER at most ONCE per conversation.
- After reading, treat those docs as cached.
- Do NOT re-open these docs unless the cache is invalidated.

## REQUIRED OUTPUT AFTER READING

Before proposing code changes, the agent must state briefly:
- **Domain authority:** which doc(s) govern this change
- **Hard invariants touched:** relevant invariants from `docs/authority.md`
- **Ownership impact:** owning thread(s) involved, if any
- **Mode impact:** affected mode(s): STOP / STATIC / REALTIME / FLASHING

If the agent cannot confidently name the governing docs or invariants:
stop and request clarification or patch docs first.

Keep this brief. Do not turn it into a ceremony dump.

---

# TOP PRIORITY: CODE-CHANGE APPROVAL GATE (ONLY BEFORE EDITING FIRMWARE CODE)

This gate is required ONLY when about to:
- write or modify firmware code
- change existing source files
- generate patches/diffs
- implement a feature or fix

It is NOT required when:
- reading files
- answering questions
- explaining concepts
- investigating or diagnosing
- asking clarifying questions
- summarizing findings
- editing documentation only

## RULE: WHEN TO USE THE GATE

Use the approval gate only if firmware code will be modified next.

Do NOT use it if only reading, discussing, or analyzing.

Do not produce a long PLAN unless code modification is imminent.

## REQUIRED APPROVAL FORMAT

Default format:

### BEFORE I CHANGE CODE
- Problem:
- Fix:
- Check:
- Risk:
- Wait for GO.

Keep it short.
Do not dump full invariant lists unless:
- the change spans multiple subsystems
- the change is architecturally risky
- the user explicitly asks for full detail

## REQUIRED FOR RISKY OR WIDE CHANGES ONLY

For larger/riskier changes, you may expand to:

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
- Docs referenced:
- Invariants touched:
- Wait for GO.

Use this expanded version only when actually needed.

## EXCEPTION: BUILD ERRORS

If the user provides a compiler/build error:
- You may immediately propose the minimal fix.
- Only modify files strictly required to resolve the error.
- Do not refactor surrounding architecture.
- Do not introduce new features.

## IMPORTANT BEHAVIOR RULES

- Do not refactor architecture unless explicitly requested.
- Do not rename threads, queues, or subsystems unless asked.
- Do not introduce optional features or “nice-to-have” changes.
- Do not silently change behavior.
- All behavior changes must be declared before editing code.
- If architectural impact is unclear, ask before proceeding.
- When replacing a function, provide the complete function body.
- Do not provide partial snippets that require searching for insertion points.

---

# AGENT RESPONSIVENESS RULES (MANDATORY)

- Never go silent while a task is active.
- If a command is blocked, aborted, or waiting for approval, report it immediately.
- While waiting on approvals or tool execution, post a short status update at least every 60 seconds.
- If repeated tool calls fail for the same reason, stop retry loops and state the blocker plus the next action.
- If progress is paused for any reason, explicitly state what is blocked and what will resume once unblocked.

---

# GIT WORKFLOW RULES (MANDATORY)

Agents may inspect and analyze git state, but must NOT perform git
state-changing actions.

The agent may help with git analysis, diffs, history, and checkpoint planning.
The agent must never create commits, branches, or otherwise mutate repository
state itself.

## ALLOWED GIT ACTIONS (READ-ONLY / ANALYSIS ONLY)

Agents may inspect git state for analysis purposes, including:
- `git status`
- `git diff`
- `git diff --cached`
- `git log`
- `git show`
- branch / HEAD inspection
- commit history analysis
- merge/conflict analysis (read-only)

## FORBIDDEN GIT ACTIONS (DO NOT EXECUTE)

Agents must NOT perform git state-changing actions.

Forbidden actions include:
- `git commit`
- `git merge`
- `git rebase`
- `git cherry-pick`
- `git revert`
- `git reset`
- `git checkout`
- `git switch`
- branch creation or deletion
- tag creation or deletion
- `git push`
- `git pull`
- `git fetch`
- stash operations
- any command that changes the working tree, index, branches, HEAD, remotes,
  or commit history

If the user asks for a git operation, the agent must still NOT execute it.
It must provide the exact command sequence for the user to run manually.

## REQUIRED MANUAL GIT HANDOFF

If a git action is needed, provide:

### GIT HANDOFF
- Reason:
- Command(s):
- Expected result:
- Warnings: (if any)

## GIT CHECKPOINT SUGGESTIONS (MANDATORY)

Suggest sensible git commit points during work, based on completed milestones.

A checkpoint suggestion should be made when:
- a bug root cause has been fixed
- a refactor completes without changing intended behavior
- a feature is implemented and locally coherent
- a doc/update/generator change is complete
- a risky change is about to begin and the current state is stable
- a debugging milestone has been reached with a known-good intermediate state

Do NOT suggest commits after every tiny edit.

Prefer one commit per coherent milestone rather than bundling unrelated changes together.

If work is unfinished or broken, do not suggest committing unless explicitly
framing it as a WIP checkpoint.

## REQUIRED CHECKPOINT OUTPUT

### CHECKPOINT SUGGESTION
- Reason:
- Suggested commit message:

Optional:
- `git add <files> && git commit -m "..."`

## OPTIONAL WIP CHECKPOINTS

### WIP CHECKPOINT SUGGESTION
- Reason:
- Suggested commit message:

---

# HARD RULES (DO NOT VIOLATE)

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

---

# TOOLING / BUILD RULES (MANDATORY)

- Agents must **NOT** run `cmake --build`, `ninja`, Cube build steps, or any full project build.
- Agents must **NOT** run long-running commands unless explicitly requested.

Allowed alternatives:
- Reason about compile errors from logs the user provides.
- Propose minimal patches with clear file/function context.
- If verification is needed, provide an explicit manual command for the user to run locally,
  and state what output to look for.

If unsure, ask first.

---

# CODE STYLE

- Prefer `static` functions and file-scope state.
- Use explicit types (`uint32_t`, `int16_t`, etc.).
- Avoid plain `int`.
- No recursion.
- Minimal macros.
- Prefer `static const` tables for configuration.
- ISR code must post to RTOS objects and return immediately.
- RTOS objects are created once at init (`MX_ThreadX_Init` / `App_ThreadX_Init`).

---

# KNOBS RULE (COMPILE-TIME TUNING SYSTEM)

All user-tunable firmware values must flow through the knobs system.

Pipeline:
- `config/knobs.json`
- `tools/gen_knobs.py`
- `Core/Inc/knobs_autogen.h`

Do NOT scatter tuning constants across source files.

## When to Add a New Knob

If a new value affects:
- gameplay feel
- UI timing
- RTOS scheduling
- power/performance tradeoffs
- debug toggles
- hardware polling rates
- hardware control values

it must be added as a knob.

Before adding a hardcoded value, ask:
Is this something the user may want to tune?

Only apply this rule to firmware-visible constants,
not private helper locals.

## Knob Hygiene Rules

- `config/knobs.json` is the only source of truth.
- Never manually edit `knobs_autogen.h`.
- Add a schema entry in `config/knobs.schema.json` with description and constraints.
- Regenerate the header after modification.

Knob definitions must remain clean and organized:
- Group related knobs logically.
- Use clear, descriptive names.
- Avoid redundant or overlapping knobs.
- Do not leave unused or deprecated knobs in the file.
- Keep formatting consistent and readable.

---

# DEBUGGING RULES

PeepShow debugging is hardware-first.

Supported interfaces:
- SWD via ST-Link V3 MINI-E
- SWO (preferred runtime visibility)
- USB CDC (optional, timing-sensitive)

UART logging is not supported.

## debug.gdb Contract


Rules:
- Use a `.gdb` for breakpoint-based debugging.
- Do not invent arbitrary breakpoint locations.
- Maximum 5 breakpoints total.
- Breakpoints must not be placed in:
  - high-frequency ISRs
  - DMA callbacks
  - display flush loops
  - audio refill loops
  - frame loop hot paths

Prefer SWO event markers over breakpoints.

## HardFault Handling

HardFault capture is mandatory.

Always extract:
- PC / LR
- stacked registers
- CFSR / HFSR
- MMFAR / BFAR (if valid)

Never treat HardFaults as “random”.

## SWO Optomisation

- SWO is available, and traceX should be used for optomisation at the appropriate stage in development. 


## STOP2 Debug Discipline

- Enable debug-in-low-power when investigating STOP2.
- Prefer SWO over breakpoints around STOP transitions.
- Do not guess about wake causes; request evidence.


---

These rules are mandatory.
Violations cause instability, non-determinism, stale edits, or useless communication.