# LPBAM Autonomous Display Validation And PeepOS Integration

Status: Hardware behavior validated; PeepOS production integration pending
Target platform: PeepShow HW5
Active-target status: HW5 result is the architecture baseline; equivalent HW6 behavior and budgets are `pending_validation`
Primary MCU: STM32U575
Primary display path: Memory-in-Pixel display over SPI3 using LPBAM / LPDMA
Primary low-power state: STOP2

## 1. Purpose

This document records the measured HW5 LPBAM display evidence and the accepted PeepOS integration architecture derived from it.

HW6 revalidation must use [[HW6_Revalidation_Matrix]] and record evidence in [[HW6_Brought_Up_Tracker]]. No measured cadence, current, wake behavior, descriptor size, row capacity, or translator behavior in this document is automatically an HW6 fact.

The Memory-in-Pixel display remains visible while the Cortex-M33 sleeps. PeepOS therefore treats low-power residency as a normal backend for settled reactive game states, not as a package-visible mode that a game must enter after an awake timeout.

The architecture goal is:

```text
Run package logic only for bounded admitted work.
Keep the CPU asleep while waiting.
Preserve or animate the waiting visual through the best target-profile backend.
Wake seamlessly for the next admitted event.
```

Package authors express gameplay state, event interests, schedules, meaningful activity, lock policy, and waiting-visual intent. Platform owns STOP2, SRAM4, LPBAM, LPDMA, SPI chunking, quiesce, wake wiring, and recovery.

Related authoritative contracts:

- [[Authority_and_Invariants]]
- [[Power_and_Sleep_Policy]]
- [[Display_and_Rendering_Contract]]
- [[Memory_and_Budgeting_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Game_Authoring_API_Contract]]
- [[Target_Profile_Schema_Contract]]

## 2. Validation Evidence

LPBAM display update experiments have shown that the display can be updated while the CPU is asleep.

Known experimental observation:

```text
SPI3 LPBAM display transfer has a 48-row transaction limit.
Larger updates must be split into chained chunks.
```

Measured LPBAM full-frame black/white alternation, 2026-06-30:

```text
SPI3 + LPBAM + LPDMA + LPTIM can alternate the full 168-row display while the CPU is in STOP2.
The practical full-frame transfer uses four 48-row-class chunks per frame.
Only the first chunk of each frame should wait on the 1 Hz trigger; the remaining chunks must run immediately.
Contiguous chunks (1-48, 49-96, 97-144, 145-168) leave three visible missing rows at the internal chunk boundaries.
Overlapping internal chunk seams removes those rows: 1-48, 48-95, 95-142, 142-168.
The final panel row also needs an end guard; duplicating the final row in the final chunk fixes the missing bottom row.
```
Measured STOP2 wake observation, 2026-06-30:

```text
Button EXTI wake from LPBAM STOP2 is functional with debug-in-STOP enabled.
The MCU entered STOP2, returned once, restored clocks, and resumed ThreadX.
EXTI4-EXTI8 were armed before STOP2 with IMR/RTSR/FTSR mask 0x1f0.
The ISR-level callback probe recorded button callbacks during STOP stage 0x5.
Example evidence: exti_callback_count=0x8, exti_callback_pin=0x100, exti_callback_button_id=0x5, exti_callback_active=0x0.
This identifies the last recorded callback as BTN_R release.
No visible wake reaction is expected yet because the experiment currently leaves the autonomous LPBAM display loop running after wake.
```
Authored frame-stitch validation experiment, 2026-06-30:

```text
The LPBAM payload generator now calls `PS_LpbamDisplay_ComposeExperimentFrames()` and slices the resulting two full-frame source buffers into the existing two-frame x four-chunk autonomous queue.
The source frame content is intentionally authored externally from sprites/UI elements rather than generated procedurally in the LPBAM slicer. The weak default composer remains a black/white fallback only until authored experiment frames are added.
This keeps visual validation under human control: the expected before/after images are known by the person inspecting the panel, so row skips, duplicated rows, chunk ordering mistakes, byte/bit-order mistakes, and frame swap timing can be judged against authored artwork.
The queue structure remains two frames x four chunks.
Expected pass: each complete authored frame appears as one coherent image; overlap seams are visually continuous; the bottom row is present; frame A and frame B alternate once per LPTIM period.
```

Measured authored-frame display validation, 2026-07-01:

```text
The validation artwork was regenerated in the native 144x168 display orientation and the converter byte order was corrected.
Authored frame A displays correctly through the normal awake display path.
Both authored frames display correctly during STOP2 LPBAM autonomous playback and alternate as expected.
This validates the current source-frame byte order, display orientation, four-chunk overlap slicing, bottom-row handling, and two-frame LPBAM playback path for the authored-frame experiment.
```

Measured reactive-to-STOP2 slice rebuild validation, 2026-07-01:

```text
Firmware rebuilds the retained LPBAM display payload each time Start requests a new STOP2 entry.
The experiment uses the same two authored validation frames, but alternates a compile variant by bit-inverting both source frames on every other LPBAM rebuild.
User observed that entering/exiting sleep causes the sleep frames to invert as expected.
This validates the reactive handoff model where Platform can regenerate retained slice payloads between waits instead of replaying stale SRAM4 content.
The experiment does not validate the final Display Program Compiler, diffing policy, partial-update packing, or package-facing visual behavior API; it only validates that the already proven LPBAM queue can be rebuilt/rearmed with different retained payload content across wake/sleep cycles.
GDB confirmation remains available through `phase6_lpbam_rebuild_count` and `phase6_lpbam_compile_variant`.
```
Measured partial-update LPBAM authored-frame cadence validation, 2026-07-01:

```text
Condition: four authored frames, normal awake frame displayed before STOP2, then LPBAM partial-frame diff payloads replayed autonomously in STOP2.
The payload compiler generated four frame transitions with dirty chunk counts {3, 3, 2, 2} and total retained wire payload length 0x22a6 bytes.
Each frame transition is structured so only the first SPI trigger/config node waits on the LPTIM edge; all chunks belonging to that frame transition then transmit as a burst.
This removed the earlier visible chunk-by-chunk scan timing and produced visually simultaneous slice updates for each frame transition.
The LPTIM setup bug was fixed by enabling LPTIM before writing ARR/CMP and waiting for ARROK/CMP1OK before starting PWM output.
Validated GDB evidence: lpbam_lptim_arr_wait_status=0x0, lpbam_lptim_cmp_wait_status=0x0, lpbam_lptim_arr_before_stop=0x1e84, lpbam_lptim_cmp_before_stop=0xf42, lpbam_lptim_cnt_after_stop=0x17e0.
With MSIK/LPTIM at 4 MHz and prescaler /128, ARR 0x1e84 gives approximately 250 ms between autonomous frame transitions.
User visual observation: cadence now looks like roughly 250 ms between frames.
Measured current during this partial-update autonomous animation was approximately 0.43 mA in the current hardware setup.
Resolved follow-up: the visible startup delay was not caused by LPBAM waiting for its first trigger. Later latency-marker testing showed the delay came from the RTOS sleep-entry software path. Once the quiesce wait/polling were bypassed for diagnostic purposes, LPBAM started essentially immediately.
```
Measured LPBAM start-latency and seeded handoff validation, 2026-07-23:

```text
Condition: four authored frames, partial-diff LPBAM payloads, STOP2 LPBAM display path prearmed before Start, ENC_CH2 used as an accessible software marker for LPBAM start.
Initial latency-only run showed that LPBAM was not the source of the visible delay: the marker and display update occurred together.
The measured software delay was traced to the normal RTOS sleep-entry path: power-owner trigger polling plus `PS_Phase6_WaitForOwnerQuiesceAcks()` with a 500-tick timeout.
Diagnostic variant 0x6A bypassed the quiesce wait and reduced the power-owner trigger poll to 1 tick; user observed LPBAM start as basically instant after Start press.
GDB evidence from the instant-start run: `lpbam_compile_variant=0x6a`, `quiesce_ack_wait_ticks=0x0`, `quiesce_ack_wait_complete=0x1`, `lpbam_start_request_tick == lpbam_dma_started_tick == lpbam_lptim_started_tick`, STOP stage remained active, and live LPTIM/SPI/LPDMA registers showed the autonomous loop running.
That run exposed the expected partial-update artifact: starting partial deltas on an unseeded display causes the first visible frame to fill in over subsequent delta frames.
Diagnostic variant 0x6B seeded authored frame 0 through the normal awake display path before STOP entry, then started the LPBAM delta loop from the seeded state.
User observed the seeded handoff as seamless: pressing Start felt like starting the animation, and the transition from live display state to autonomous STOP animation was visually acceptable.
Architecture implication: a normal PeepOS reactive-wait handoff should present or preserve the current base MIP during the settling transaction, prebuild/select an LPBAM delta sequence for the matching waiting visual, then start LPBAM from the next transition. LPBAM does not need to perform a full-frame startup write in the normal handoff path.
Open follow-up: normal production sleep entry must replace the latency-only quiesce bypass with a bounded owner protocol that preserves responsiveness without racing peripheral shutdown.
```
Measured non-debug wake classification and wake-to-reactive cleanup observation, 2026-06-30:

```text
Condition: full-frame black/white LPBAM playback, debug-in-STOP disabled, wake by A button.
STOP2 was attempted and returned once; clock restore completed.
Button wake inputs were rearmed before STOP2 with EXTI IMR/RTSR/FTSR mask 0x1f0.
Wake reason classification recorded wake_reason=0x1, wake_button_id=0x2, wake_pin=0x20, wake_active_level=0x1, wake_stage=0x5.
LPBAM abort path was attempted after wake; LPTIM stop, DMA abort, DMA unlink, and SPI abort all returned HAL_OK.
DMA error after abort was 0x20, matching HAL_DMA_ERROR_NO_XFER after aborting an autonomous list that had already stopped at the sampled point.
A follow-up run after restoring SPI3 out of autonomous mode validated wake by B button with wake_reason=0x1, wake_button_id=0x3, wake_pin=0x40, wake_stage=0x5, and lpbam_post_wake_marker_status=0x0. Normal display-path restoration after LPBAM abort is therefore validated for this diagnostic marker path.
```
Measured non-debug LPBAM STOP2 current baseline, 2026-06-30:

```text
Condition: full-frame black/white LPBAM display alternation, debug-in-STOP disabled.
After boot / awake idle current: 2.86 mA.
After pressing Start / STOP2 autonomous display current: 0.36 mA.
After pressing A / awake current after wake: 2.234 mA.
Wake/sleep/wake cycling is functional in this experiment.
This is experimental validation evidence only; it is not the final low-power architecture budget.
```
Measured non-debug static STOP2 control baseline, 2026-06-30:

```text
Condition: same STOP2/wake path, debug-in-STOP disabled, LPBAM autonomous display playback not started.
Post-Start settle / awake current: 2.86 mA.
Post-Start STOP2 sleep current: 0.315 mA.
Post-A wake current: 2.00 mA.
Compared with full-frame LPBAM playback at 0.36 mA, the measured autonomous display playback overhead is about 0.045 mA in this setup.
```
Validated SRAM4 full-frame LPBAM budget, 2026-06-30:

```text
Build: fw0 LPBAM full-frame autonomous playback enabled.
Total SRAM4 used by image: 13,528 B.
Static STOP2 control image SRAM4 use: 3,368 B.
Incremental autonomous-display SRAM4 cost: 10,160 B.
Allocated display TX payload buffers: 7,864 B.
Actual black or white frame wire payload length: 3,452 B.
Two actual frame payloads therefore represent 6,904 B of useful wire data.
Allocated LPBAM/LPDMA descriptor and queue objects: 2,232 B.
Observed alignment/padding/other incremental overhead: about 54 B.
Current full-frame two-state experiment uses about 82.6% of SRAM4, so the final architecture needs partial/sprite slice packing rather than many full-frame states.
```
Measured current SRAM4 LPBAM budget after marker removal, 2026-07-23:

```text
Build: fw0 LPBAM authored partial-diff experiment, compile variant 0x6C, diagnostic GPIO marker path removed.
Total SRAM4 region: 16,384 B.
Current linked SRAM4 use: 15,544 B.
Current linked SRAM4 free: 840 B.

Measured SRAM4 objects from ELF/map:
- Awake display SPI TX buffer `txBuf`: 3,362 B.
- Alignment after `txBuf`: 2 B.
- LPBAM display metadata: 66 B.
- Fixed LPBAM retained payload arena: 9,216 B.
- Alignment before LPBAM descriptors: 26 B.
- SPI chunk descriptors: 12 descriptors x 236 B = 2,832 B.
- Alignment before queue object: 16 B.
- `Queue1_Q`: 24 B.

Current SRAM4 model:
  used = align32(3362 + align4_pad + 66 + payload_arena)
       + (chunk_count * 236)
       + align32_pad_before_queue
       + 24

MIP display wire row size:
- `LINE_WIDTH` is 18 B for 144 px at 1 bpp.
- One transmitted row is gate byte + 18 data bytes + trailing byte = 20 B.
- One LPBAM chunk payload is command byte + N rows + duplicated guard row + two trailing bytes.
- Chunk payload bytes = 20*N + 23, rounded/aligned by the SRAM4 payload allocator before the next chunk.
- Conservative row-capacity estimate: `floor((payload_arena - 24*chunk_count) / 20)`.

Current configured experiment limit:
- Descriptor capacity: 4 frames x 3 chunks = 12 chunks.
- Payload arena: 9,216 B.
- Approximate retained dirty-row capacity with 12 chunks: 446 rows.
- Current authored diagnostic animation uses 6 active chunks and about 4,258 B of payload, so the fixed arena is currently much larger than the active payload.

Theoretical no-reserve SRAM4 capacity if descriptor count is fixed at build time:
- 4 chunk descriptors: payload arena up to 11,962 B, about 593 dirty rows.
- 8 chunk descriptors: payload arena up to 11,034 B, about 542 dirty rows.
- 12 chunk descriptors: payload arena up to 10,074 B, about 489 dirty rows.
- 16 chunk descriptors: payload arena up to 9,146 B, about 438 dirty rows.
- 20 chunk descriptors: payload arena up to 8,186 B, about 385 dirty rows.

Architecture implication: SRAM4 should be treated as a byte budget, not a frame budget. Each extra possible chunk costs roughly 236 B of descriptor SRAM before any display payload is stored. The final compiler should reserve a descriptor count and payload arena together, keep a safety margin, and prefer fewer merged row spans when the descriptor cost outweighs the payload savings.
```

Production SRAM4 allocation decision, 2026-07-23:

```text
Decision: SRAM4 belongs to the display-DMA/autonomous LPBAM arena. RTOS, game resume state, source frames, and the committed framebuffer live in another retained RAM bank.

The awake display SPI staging buffer `txBuf` still needs SRAM4 residency for the validated LPDMA display path. It should not be treated as unrelated runtime RAM. Instead, production should make it an overlay/scratch allocation inside the same SRAM4 display arena. When the system is doing an awake display present, that region is a DMA TX staging buffer. Once the autonomous slice is compiled and armed, the same SRAM4 bytes may be retained LPBAM payload.

Recommended initial production LPBAM SRAM4 partition in overlay mode:
- Total SRAM4: 16,384 B.
- Guard/reserve: 1,024 B.
- Slice metadata, rounded: 96 B.
- DMA queue object, rounded: 32 B.
- Max SPI chunk descriptors: 16 x 236 B, rounded to 3,776 B.
- Retained wire-payload arena, including reusable display TX scratch: 11,456 B.
- Required awake display TX scratch window inside that arena: 3,364 B.

This gives a conservative dirty-row capacity of about 553 rows per autonomous slice when the TX scratch is overlaid with payload:
  floor((11,456 - 16*24) / 20) = 553 rows

That is about 3.3 full-display equivalents of changed rows, before considering that real sprite/idle loops usually touch far less than the whole display.

If the architecture ever requires a fully prebuilt autonomous slice and an independent awake TX staging buffer to coexist in SRAM4 at the same time, the 16-chunk payload budget drops to about 8,092 B, or about 385 dirty rows. Avoid that mode unless a specific latency requirement proves it is necessary.

Sequencing rule: the display owner must not use the SRAM4 TX scratch after the slice has been armed unless it first aborts/unlinks LPBAM or rebuilds the slice afterward. Preferred reactive-wait order is: render/commit in retained runtime RAM, present the seed frame using SRAM4 scratch, compile/pack the waiting-visual payload into the SRAM4 arena, arm LPBAM, then let Platform enter STOP2.
Why 16 chunks:
- A full-display transition needs four 48-row-class chunks on the current display path.
- 16 chunks therefore supports up to four full-screen-class transitions, or more practical partial transitions with 1-2 chunks each.
- Increasing to 20 chunks costs another 960 B of descriptor SRAM and drops payload capacity to about 500 dirty rows with a 1 KB reserve.
- Decreasing to 12 chunks gains about 944 B of payload, roughly 52 conservative changed rows, while reducing worst-case transition flexibility.

Accepted initial PeepOS v0 slice limits:
- `max_lpbam_spi_chunks_per_slice = 16`.
- `max_lpbam_payload_bytes_per_slice = 11,456`.
- `sram4_guard_bytes = 1,024`.
- `max_rows_per_spi_chunk = 48`.
- `wire_bytes_per_display_row = 20`.
- `chunk_payload_overhead_bytes = 23` before allocator alignment.
- `chunk_descriptor_bytes = 236`.

Compiler rule: decide whether to merge row spans by comparing saved descriptor cost against added row payload. Since one descriptor costs 236 B and one extra transmitted row costs 20 B, merging gaps of roughly 11 rows or fewer is usually cheaper than creating another chunk. The compiler should still obey the 48-row transaction cap and seam/guard handling.
```


The core LPBAM display behavior, button wake, abort/restore path, partial-diff cadence, seeded reactive-wait handoff, and SRAM4 budget model are validated on STM32U575. Production integration remains pending where listed below; the architectural semantics in this document are accepted.

## 3. Accepted Execution Architecture

### 3.1 Package-facing execution semantics

PeepOS exposes two execution semantics to Engine/package tooling:

| Semantic | Meaning |
|---|---|
| `REACTIVE` | Handle one admitted input, schedule, sensor, lifecycle, or system event through a bounded transaction, then yield immediately. |
| `REALTIME` | Run a frame-paced active loop while admitted meaningful work continues, with declared budgets and a reactive fallback. |

`STATIC` is not an execution-mode token. Static art, a static frame, or an internal one-shot display update may still use the ordinary adjective where accurate.

STOP2, LPBAM, LPDMA, display DMA, and SRAM4 are Platform implementation details. A package does not request or observe them as gameplay modes.

### 3.2 Reactive transaction cycle

A reactive state follows this cycle:

```text
admitted event
  -> restore lifecycle and focus if required
  -> execute bounded state/action work
  -> render and commit the settled logical frame
  -> resolve the state's waiting visual and fallback
  -> publish event interests, schedules, and symbolic wake intents
  -> yield
  -> Platform selects the deepest compatible sleep/backend
```

Waiting for input is not an awake mode. The mounted package and its committed state remain retained while the CPU sleeps.

A waiting visual may be:

- a hold of the committed frame
- a bounded sequence of final logical visual states
- a reduced sequence
- a hold fallback

Waiting-visual motion is cosmetic presentation. It does not execute package logic, mutate game variables, or advance committed game state.

### 3.3 Realtime execution

`REALTIME` remains CPU-awake and frame paced. It is used for gameplay or presentation that genuinely needs continuous Engine work.

Every realtime unit declares:

- target cadence and frame budget
- meaningful-activity sources
- suspend/resume behavior
- a reactive fallback
- any bounded input-lock deferral needed for an indivisible segment

When realtime work ends, blocks, or loses admission, the unit follows its declared reactive route. It does not remain awake merely to wait for input.

## 4. Input Lock And Gameplay Timeouts

Automatic input locking is optional package policy. A package may disable it.

When enabled:

- only Start wakes/unlocks normal interaction
- the Start press is consumed by PeepOS and is not also delivered as a package action
- Engine receives symbolic lock/unlock lifecycle events after state/focus ordering is valid
- the package chooses one admitted lock route: preserve current state, transition to a declared package state, or exit to shell
- declared meaningful input, including admitted gyro activity where appropriate, may refresh the timer
- cosmetic animation does not refresh the timer
- lock deferral must be statically bounded; unbounded deferral is forbidden

Input lock and physical sleep are orthogonal. A reactive package normally sleeps while unlocked whenever it is waiting. Locking changes admitted input/focus policy, not whether low power is allowed.

A designer-authored inactivity behavior, such as `explore -> pet_idle`, is a normal gameplay schedule and state transition. It is separate from the PeepOS input lock.

## 5. Display And Memory Model

### 5.1 Committed state and framebuffer

Committed game state and the committed logical framebuffer are retained outside SRAM4. They are the recovery truth after wake, interruption, abort, or uncertain panel state.

The physical display may move through waiting-visual states while the committed game state remains unchanged.

### 5.2 SRAM4 display-DMA/autonomous arena

All 16 KiB of SRAM4 belongs to the Platform display-DMA/autonomous arena. It contains:

- awake display TX scratch as an overlay within the payload arena
- compiled waiting-visual wire payloads
- LPDMA/LPBAM descriptors
- queue and slice metadata
- alignment and guard space

RTOS objects, package state, source frames, and the committed framebuffer live in another retained RAM bank.

Accepted initial partition:

| Item | Bytes |
|---|---:|
| SRAM4 total | 16,384 |
| guard/reserve | 1,024 |
| slice metadata, rounded | 96 |
| queue object, rounded | 32 |
| 16 descriptors x 236 B | 3,776 |
| payload arena including reusable TX scratch | 11,456 |
| required awake TX scratch window inside payload arena | 3,364 |

Conservative changed-row capacity:

```text
floor((11,456 - 16*24) / 20) = 553 rows
```

That is about 3.3 full-display equivalents of changed rows. It is not a guaranteed frame count: real capacity depends on changed spans, descriptor use, guard/seam handling, and cadence structure.

### 5.3 Overlay sequencing invariant

The display owner may use the overlay as awake TX scratch before the autonomous slice is armed. After arming, it must not overwrite that memory until LPBAM is aborted/unlinked or the slice is intentionally rebuilt.

Canonical handoff:

```text
render and commit logical seed frame in retained runtime RAM
  -> present seed frame through SRAM4 TX scratch
  -> compile/pack waiting-visual deltas into the SRAM4 arena
  -> arm the autonomous list
  -> yield to Platform sleep policy
```

The measured seeded handoff is visually seamless.

## 6. Display Program Compiler

The Platform-owned Display Program Compiler converts a portable waiting-visual contract into a target-specific autonomous slice.

It may:

- resolve final logical 1bpp visual states
- diff consecutive states against the committed seed
- extract and merge changed row spans
- split transfers at the measured 48-row limit
- apply the measured seam overlap and final-row guard handling
- compare descriptor cost against added row payload
- pack payload, descriptors, timing, and recovery metadata into the SRAM4 arena

Initial internal limits:

- 16 SPI chunks per slice
- 11,456 payload bytes per slice, including reusable TX scratch
- 1,024 guard bytes
- 48 rows per SPI chunk
- 20 wire bytes per display row
- 23 bytes fixed payload overhead per chunk before alignment
- 236 bytes per descriptor

Because one descriptor costs 236 B and one transmitted row costs 20 B, merging a gap of roughly 11 rows or fewer is usually cheaper than adding a descriptor, subject to the 48-row transfer cap and measured seam rules.

Normal package tools must not expose rows, chunks, SRAM4, descriptors, wire bytes, or LPBAM. They report portable compatibility limits such as waiting-visual complexity, changed-area budget, sequence timing, fallback use, and target compatibility.

## 7. Authoring Integration

Authoring blocks and Authoring Kits expose gameplay semantics, not a separate low-power graph.

Examples include:

- Menu
- Dialogue
- Inventory
- Turn-Based Encounter
- Pet Idle
- Explore
- Clock/Status View

A block compiles conceptually to:

```text
reactive_block:
  entry_actions[]
  event_handlers[]
  state_transitions[]
  settled_view_ref
  waiting_visual_ref
  waiting_visual_fallback_ref
  event_interests[]
  schedules[]
  gameplay_timeout_transitions[]
  input_lock_context_ref
  bounds
```

A Menu block therefore handles input, changes selection, renders the settled menu, publishes the next wait contract, and yields. Its cursor or background may continue through an admitted waiting-visual backend while the CPU sleeps. The next input resumes the same mounted menu and executes another bounded transaction.

This creates useful target-derived authoring constraints. A waiting visual may be rejected, reduced, or held when its compiled changed-area, sequence, timing, or fallback requirements exceed the selected target profile. The author sees those constraints in PeepOS visual terms, never hardware terms.

## 8. Target Profiles And Fallback

Target profiles publish portable facts derived from Platform evidence.

| Profile behavior | Reactive waiting result |
|---|---|
| waiting-visual animation granted | compile eligible waiting motion into the measured autonomous backend |
| waiting-visual animation unavailable | use declared reduced sequence or hold; baseline profile may use admitted wake/update/yield behavior where specified |

The measured `16`-chunk, `11,456 B`, and approximately `553`-changed-row limits remain in the Platform compiler profile and hardware evidence. The package-facing target profile publishes the waiting-visual grant, authored frame/cadence/cycle bounds, a versioned compiler profile ID, and abstract admission/utilization results. Normal authoring tools do not expose rows, chunks, descriptors, SRAM4, or LPBAM.

Packages remain portable. They contain logical visual states and intent, not panel rows, SPI payloads, SRAM addresses, DMA descriptors, or LPBAM nodes.

A package that requires continued waiting motion must require `display.waiting_visual_animation`. An optional preferred sequence must declare a reduced sequence or hold fallback.

## 9. Production Integration Still Pending

The architecture is accepted, but these implementation/evidence items remain open:

- replace the diagnostic quiesce bypass with a bounded owner acknowledgement protocol that preserves the measured responsive handoff
- validate the production wake-source mix and owner restoration path
- define autonomous-slice completion, timeout, abort, and error recovery
- implement and validate the production Display Program Compiler
- freeze target-profile waiting-visual limits from measured builds
- measure final release current with production instrumentation removed
- validate retained runtime/committed-framebuffer placement in the selected non-SRAM4 bank
- add compatibility-report and authoring-tool diagnostics for waiting-visual budget/fallback outcomes

Record completion evidence in [[Brought_Up_Tracker]] and unresolved constants in [[Pending_Measured_Constants_Register]].

## 10. Decision Summary

The LPBAM experiment answered the architectural question:

- PeepOS should not keep state-based gameplay awake while waiting for input.
- Reactive gameplay is the default execution model, not a special low-power mode.
- Waiting visuals are attached to settled reactive states.
- Platform may realize waiting motion with LPBAM/LPDMA in STOP2 when the target profile grants it.
- Seeded awake-to-autonomous handoff is the canonical display transition.
- SRAM4 is a shared display-DMA/autonomous arena whose TX scratch overlays retained payload storage.
- Package automatic input locking is optional; when enabled, Start-only unlock is consumed and all deferrals are bounded.
- Realtime remains available for work that genuinely needs continuous CPU execution and always declares a reactive route back.
