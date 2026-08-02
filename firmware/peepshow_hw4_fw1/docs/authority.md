# Authority: Cross-Cutting Invariants (PEEPSHOW THREADX)

This document is the **single source of truth** for project-wide invariants that apply across all subsystems.
If any other document conflicts with this one, **this document wins**.

The goal is to prevent “helpful” refactors that accidentally break power, timing, ownership, or determinism.

---

## 0. Canonical naming

### 0.1 Modes (exact tokens)
All docs + code must use these exact mode names:

- **STOP**: low-power pet runtime (STOP2-first), RTC cadence.
- **STATIC**: awake UI/menu mode (interactive, but not 30 FPS).
- **REALTIME**: 30 FPS game loop mode (frame-scheduled, performance-critical).
- **FLASHING**: USB MSC / update mode (host-facing storage operations allowed).

### 0.2 System clocks (naming)
Clock profiles (if used) must be named explicitly and consistently (example tokens):

- `CLK_LOW` (STOP/STATIC typical)
- `CLK_NORM` (STATIC interactive)
- `CLK_TURBO` (REALTIME bursts)

If you add additional profiles, update this document.

### 0.3 Canonical BOM part names
The following part names are **canonical tokens**. All other docs must match them exactly.

- Display: **Sharp Memory LCD LS013B7DH05**
- MCU: **STM32U575**
- External flash: **AT25SL128A** (OctoSPI / QSPI)
- Audio amp: **MAX98357A** (I2S)
- IMU/Accel: **LIS2DUX12**
- Hall joystick: **TMAG5273** 

If any of the above differs per hardware revision, create a `BOM_REV.md` and point here.

---

## 1. Ownership model (non-negotiable)

### 1.1 Single-owner rule
Every hardware peripheral or shared subsystem has **exactly one owner thread**.
No other thread may call HAL/LL or touch registers for that peripheral.

Examples of “owned things”:
- SPI instance used by display
- I2C bus used by sensors
- SAI/I2S audio path
- OctoSPI/QSPI flash controller
- USB stack / FileX mount state
- Power/PMIC I2C interface (if shared, it must still be single-owned)

### 1.2 Requests, not calls
Other threads interact with a peripheral **only by sending a request** to the owner via queue/event.

#### Request payload rules (to prevent foot-guns)
Request structs MUST be:
- Plain old data (fixed-width ints, enums)
- No HAL handles
- No function pointers
- No pointers to transient stack buffers
- If a pointer is unavoidable (rare), the lifetime/ownership must be explicit and guaranteed

All requests must represent **bounded work** (no “run forever” requests).

---

## 2. Timing model (STOP / STATIC / REALTIME)

### 2.1 STOP cadence source
**STOP runtime cadence is RTC-driven**, not SysTick-driven.

- STOP updates are triggered by RTC alarm / wake scheduling.
- STOP logic must never assume SysTick is running or accurate during STOP2 usage.

### 2.2 REALTIME cadence source
**REALTIME cadence is frame-scheduled** (e.g. TIM2 “frame flag” @ 30 FPS).
The frame scheduler defines the pacing; everything else is a follower.

### 2.3 STATIC cadence
STATIC may use RTOS timers and/or SysTick as convenient, but must not degrade STOP2 entry/exit.

---

## 3. Clock transitions (must not break time)

Clock transitions are allowed, but they must obey:

### 3.1 SysTick + HAL tick correctness
If SYSCLK/AHB changes, the system must ensure:
- SysTick is reprogrammed to the correct period
- HAL_GetTick remains valid (or intentionally disabled with a documented replacement)

### 3.2 ThreadX timebase correctness
If ThreadX relies on a timebase that is impacted by clock changes, the transition sequence must include
whatever is required so ThreadX timers/sleeps behave correctly post-transition.

**Rule:** no mode transition is “complete” until timebases are sane.

---

## 4. STOP2 entry/exit discipline

### 4.1 “Quiesce then sleep”
Before STOP2 entry, each subsystem must quiesce:
- Stop / pause DMAs cleanly
- Park peripherals into a known low-power state
- Ensure no worker thread continues to enqueue work that cannot run in STOP2

### 4.2 “Resume then validate”
After wake, each subsystem must:
- Re-init any peripheral that is not guaranteed retained
- Validate liveness (e.g. sensor WHOAMI read, display “ping” pattern, etc.)
- Fail gracefully if not present (see Section 11)

---

## 5. Display invariants (Sharp Memory LCD)

These are unusual enough that they must be defended against “normalization”:

- SPI is **LSB-first**
- Display CS is **active-HIGH**
- Row addressing is **1-based**
- No “rev8” / bit reversal applied during flush (unless explicitly documented and tested)

Any code that changes SPI bit order, CS polarity, or row addressing must include
a bring-up test and must be treated as a breaking change.

---

## 6. Storage invariants (FileX vs raw region)

### 6.1 Runtime prohibition
During **REALTIME**, FileX is treated as **unavailable**:
- No mount/unmount
- No open/read/write/dir operations
- No FAT browsing
- No streaming from FAT

### 6.2 Allowed runtime access
REALTIME is allowed to read only from **installed raw asset regions** (pre-packed, fixed offsets).

### 6.3 FLASHING is the exception
FLASHING mode may use FileX / USB MSC and perform host-facing storage operations.

---

## 7. Audio invariants

- Audio in REALTIME must be **bounded CPU** and **non-blocking** (DMA double buffer, owner-thread control).
- No “read from FAT every buffer refill”.
- If audio assets are used, they must be installed into raw regions before REALTIME usage.

---

## 8. Logging & debug invariants

### 8.1 UART console policy
**No UART console logging.** UART is not to be used as a general logging channel.

### 8.2 Allowed debug channels
Allowed channels (developer-only):
- SWO / RTT (preferred for structured debug)
- USB CDC (optional)

### 8.3 USB CDC is not a printf firehose
If USB CDC exists:
- Must be rate-limited
- Must not be used in ISRs
- Must not be used in hot REALTIME paths
- Must be safe to disable entirely at compile time

---

## 9. Compile-time knobs policy

Knobs exist to tune constants without hunting scattered defines.
Knobs must not introduce hidden runtime variability.

Allowed knob categories:
- Buffer sizes, queue depths, pool sizes
- Feature toggles (compile-time)
- Thresholds/cooldowns for documented algorithms (e.g., power governor thresholds)

Not allowed:
- Knobs that add retries/backoff loops that change timing unpredictably
- Knobs that silently change mode cadence sources

---

## 10. Determinism policy

REALTIME must remain predictable:
- No unbounded loops
- No blocking waits on peripherals in the frame path
- No filesystem operations
- No “best effort” background tasks stealing the frame budget

If something needs to run “eventually”, it must be scheduled outside the frame critical path.

---

## 11. Robustness policy (peripheral hardening)

Peripherals can fail to come up at boot or after wake. The system must tolerate this.

Required behavior:
- Drivers expose a state machine: `OFFLINE -> INIT -> ONLINE -> ERROR` (names can vary)
- The owner thread periodically attempts recovery with bounded retries
- Recovery must not block other system progress
- Mode transitions can be delayed only with explicit timeouts (no infinite waits)

If a peripheral is required for a mode (e.g. joystick in REALTIME), the mode entry must:
- Validate dependency list
- Fail entry cleanly (fallback, message, or return to STATIC)

---

## 12. Document priority

When implementing:
1. `authority.md` (this doc)
2. Subsystem doc (display/audio/storage/power/rtos/etc.)
3. README / bring-up guides
4. Code comments

If a conflict exists, resolve it by updating docs so the conflict disappears.

---

## 13. “Breaking change” checklist

Any change that touches the following MUST include an explicit test and doc update:
- STOP2 entry/exit sequence
- Clock profile switching
- Display SPI config (bit order / CS polarity)
- ThreadX tick/timebase behavior
- FileX mounting rules / raw region layout
- Audio buffer pacing / sample rate / DMA strategy
- Update/install pipeline formats

---

Last updated: 2026-02-20