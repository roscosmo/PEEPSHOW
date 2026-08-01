# PPK2 Console And Capture Tool

`tools/ppk2_console.py` is the normal visible operator surface for PPK2 source power and current capture during active-target bring-up, currently HW6. It controls `tools/ppk2_service.py`, which is the sole owner of the PPK2 serial port while a PeepShow session is active.

Related:

- [[Power_Measurement_and_Trace_Correlation_Runbook]]
- [[Evidence_Artifact_Convention]]
- [[Dev_Orchestration_CLI_Contract]]

## Ownership

A PPK2 serial port can be owned by one application at a time.

- **Released**: Nordic Power Profiler may use the PPK2.
- **PeepShow console owns PPK2**: the console controls target source power, live telemetry, and capture. Nordic's application cannot connect.
- **Capture active**: the console owns the PPK2 and writes a CSV plus metadata under `PPK2_logs/`.
- **Release PPK2**: stops any capture, cuts source power, stops the service, and frees the COM port for Nordic's application.

The console is intentionally the visible ownership indicator. When it starts the service, the service runs without a separate command window. Closing the console releases the PPK2 and turns Target source power off.

## Setup

```powershell
py -m venv tools/.venv
tools/.venv/Scripts/python.exe -m pip install -r tools/requirements-ppk2.txt
```

The PPK2 API source is vendored at `tools/vendor/ppk2-api-python` and is GPL-2.0 licensed. It is host-side development tooling only.

## Console

```powershell
tools/.venv/Scripts/python.exe tools/ppk2_console.py
```

Use **Acquire PPK2** to take `COM10` (or the selected port) with Target source power off. Use **Power Target On** and **Power Target Off** independently while PeepShow retains the port. The console provides:

- separate instrument ownership and Target source-power state
- live current trace retained since Target source power-on or **Reset Plot**
- large direct source-power toggle with no confirmation step
- **RESET TARGET** power-cycle control, which turns source power off briefly and then back on without releasing the PPK2
- selectable `D1` through `D7` logic monitor, with a live HIGH/LOW indicator, edge count, and a digital trace aligned to the current plot
- selectable full-trace or latest 0.5, 1, 2, 5, 10, or 20 second plot view
- visible recording indicator while CSV capture is active
- recent 1 second current summary plus session mean, minimum, maximum, RMS, charge, energy, and sample count since power-on or plot reset
- labelled bounded captures
- last CSV artifact path
- explicit release of the serial port and target source power

The live plot is a downsampled operator view retained in memory for the current powered session. Each GUI poll requests only the selected time window and a display-sized point budget; the service preserves bucket current extrema, final logic state, and accumulated edge masks while reducing the response. This keeps the console responsive without changing the saved CSV. **Reset Plot Data** clears the operator trace, live current statistics, and charge/energy integration without stopping source power; older live samples are ignored after reset. **RESET TARGET** stops any active capture, turns target source power off for about 0.5 s, turns it back on, and starts a fresh live session. The CSV remains the source artifact for analysis.

The logic selector defaults to `D7`. Channel `Dn` maps to bit `n` in the PPK2 API's per-sample `digital_bits` value. The plotted logic level is sampled in the same 10 ms buckets as the 100 point/s operator current trace. An orange marker means at least one edge occurred inside that bucket, including a pulse that returned to its previous level before the bucket ended. The edge count is exact for samples received by the service. All eight raw bits remain in each CSV row; use a `100 kS/s` CSV capture when exact edge timing matters.

The PPK2 logic header needs its own target-side logic reference and ground. The
source-meter `VOUT` connection does not implicitly provide the logic-header
`VCC`. Connect the target IO rail to logic `VCC`, target ground to logic `GND`,
and the marker signal to the selected `D1` through `D7` input. For HW6,
`PWR_DBG` is a 1.8 V signal, so use the target 1.8 V rail as the logic reference.
Do not connect a logic reference outside the PPK2 input specification.

**Reset Plot Data** also resets the displayed logic edge count and logic history. Changing the selected logic input does not reset or alter captured data.

Charge and energy are operator estimates from integrated PPK2 samples: current is integrated against the nominal PPK2 sample rate to produce `mAh`, and `mWh` is derived from that charge and the configured source voltage. Use these for quick bring-up comparisons; use saved CSV artifacts for final evidence analysis.

## CLI

The CLI remains useful for scripted, bounded scenarios.

```powershell
# Discover the PPK2 control port.
tools/.venv/Scripts/python.exe tools/ppk2_capture.py list

# Start a titled service console. Use the graphical console for the normal interactive workflow.
tools/.venv/Scripts/python.exe tools/ppk2_service.py power --port COM10 --voltage-mv 3300 --state on

# Read ownership, current capture state, and capture statistics.
tools/.venv/Scripts/python.exe tools/ppk2_service.py status

# Start and wait for a bounded capture without dropping Target power.
tools/.venv/Scripts/python.exe tools/ppk2_service.py capture --seconds 30 --label stop2-baseline

# End an active capture while retaining source power.
tools/.venv/Scripts/python.exe tools/ppk2_service.py capture-stop

# Stop the service, cut Target power, and release COM10.
tools/.venv/Scripts/python.exe tools/ppk2_service.py power --port COM10 --voltage-mv 3300 --state off
```

The service listens on loopback only (`127.0.0.1:49365`). A stale session marker is removed only after the local service cannot be reached; it does not alter PPK2 source power.

Each capture writes a CSV and JSON metadata manifest under `PPK2_logs/`. A capture is electrical evidence only; correlate it with Platform markers and a defined scenario before attributing current to a firmware subsystem.

## Safety

- Power the target from one source only during a PPK2 source-meter capture.
- Keep SWD ground connected, but do not let the debugger supply target power.
- Record board revision, firmware commit, source voltage, current range, and test scenario in the evidence manifest.
- Use **Release PPK2** before opening Nordic's profiler.
- Do not close an older titled service console while the target is being flashed, debugged, or measured. Its cleanup turns source power off.








