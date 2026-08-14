# Power Validation

Record measured power behavior here.

Each entry must include:

- date and time
- board revision
- firmware commit
- knobs/configuration hash or version
- runtime class, `REACTIVE`/`REALTIME` semantic, and deterministic workload ID
- physical backend, sleep class, and armed wake sources
- internal operating-point ID plus SYSCLK/HCLK, voltage scale, flash/cache state, and relevant kernel clocks
- instrumentation profile and external measurement configuration
- average and peak current, measured duration, and charge/energy over the defined window
- wake, resume, first-response, and settled-presentation latency where applicable
- failed wake/resume/transaction count
- artifact links

Reactive operating-point evidence must cover the entire admitted-event-to-yield cycle, including presentation preparation and return to stable wait. Selection is based on transaction charge/energy subject to response-latency and correctness limits, not active current alone.

Realtime operating-point evidence must include frame-time distribution, worst-case deadline margin, missed frames, audio underruns/glitches, representative contention, and sustained current. Selection is the lowest-power point that meets all admitted deadlines with margin.

Any dynamic switching policy must separately record transition latency, transition charge/energy, failure behavior, and the break-even interval used for hysteresis.

Unknown wake reasons remain defects until explained.

## Current HW6 FW0 Evidence

- `EV-HW6-20260812-P1-CLOCKUSB-037` validates the first power-owned clock-policy handoff for USB MSC export/reclaim on `HW6-UNIT-001`. The export path selected `CLK_IO_HIGH`/USB capability and mounted with `SystemCoreClock=160000000`; reclaim restored base profile `CLK_REACTIVE_BASE`, `SystemCoreClock=24000000`, and `usb clk/vdd/hsi48 = 0 / 0 / 0`. This is correctness evidence for the USB path, not current, energy, reactive/realtime operating-point, or hysteresis evidence.

- `EV-HW6-20260813-P1-STOP2CTRL-053` validates default-off controlled STOP2 entry on `HW6-UNIT-001`. `thPower` first reported eligibility status/block/pending `0x0 / 0x0 / 0x3`, then entered real STOP2 once, woke from physical START, restored clocks, and reported owner quiesce/enter/clock/recover/last statuses all `0x0`. This is correctness evidence for admission-gated STOP2 entry and wake recovery, not current, energy, LPBAM waiting visuals, tick compensation, or repeated-cycle evidence.

- `EV-HW6-20260814-P1-STOP2HELD-061` validates the default held-frame display wait backend on `HW6-UNIT-001` using the automatic STOP2 idle dry-run. Probe API `31` reported backend request/selected/status/held `1 / 1 / 0x0 / 1`, stable display state, clear hard blockers, eligibility pending only owner quiesce, and dry-run pending `OWNER_QUIESCE | IDLE_WINDOW`; no real STOP2 entry occurred. This is correctness evidence for display-wait admission policy, not current, LPBAM playback, real automatic entry, or repeated-cycle evidence.

- `EV-HW6-20260814-P1-STOP2AUTOENTRY-062` validates explicit one-shot held-frame automatic STOP2 entry on `HW6-UNIT-001`. Probe API `32` reported forced debug auto-entry `1 / 1`, auto entry status `0x0`, held-frame backend `1 / 1 / 0x0 / 1`, owner STOP2 count `0 -> 1`, quiesce/enter/clock/recover all `0x0`, debug-low-power disabled, `PWR_SR.STOPF = 0x2`, PA4 IDR `0x6055 -> 0x6045`, and START wake source/primary `0x1 / 1`. This is correctness evidence for the explicit held-frame auto-entry path and START wake/resume, not current, production periodic auto-sleep, LPBAM playback, or repeated-cycle evidence.

Related:

- [[Power_and_Sleep_Policy]]
- [[HW6_Clock_Tree_Contract]]
- [[Power_Measurement_and_Trace_Correlation_Runbook]]
- [[Pending_Measured_Constants_Register]]
- [[HW6_Brought_Up_Tracker]]
- [[HW6_Wake_Sources]]
