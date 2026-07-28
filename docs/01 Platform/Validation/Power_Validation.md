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

Related:

- [[Power_and_Sleep_Policy]]
- [[HW6_Clock_Tree_Contract]]
- [[Power_Measurement_and_Trace_Correlation_Runbook]]
- [[Pending_Measured_Constants_Register]]
- [[HW6_Brought_Up_Tracker]]
- [[HW6_Wake_Sources]]
