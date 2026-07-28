# Bounded Build, Flash, and Debug Runbook

This runbook defines the only approved pattern for coding agents to build, flash, and inspect firmware during PeepShow bring-up.

The goal is to allow useful autonomous work without returning to the historical failure mode where an agent starts an unbounded build, flash, GDB server, or GDB session and hangs indefinitely.

This runbook is based on successful earlier-revision experiments. It is an automation safety rule for active-target work, currently HW6, and is not hardware evidence by itself.

Related:

- [[Debug_Workflows]]
- [[Bootstrap_and_Build]]
- [[Brought_Up_Tracker]]
- [[Evidence_Artifact_Convention]]
- [[USB_MSC_Bring-up_and_Recovery_Runbook]]

---

## Core Rule

Agents may build, flash, and run debugger inspection only through bounded wrappers.

Every wrapper must:

- set a hard timeout
- capture stdout and stderr
- report elapsed time
- report exit code
- kill the process tree on timeout
- clean up started helper processes
- check for leftover relevant processes after completion

Agents must not start raw interactive firmware tools directly.

---

## Human Approval Boundary

Build-only commands may be run when they are relevant to the requested task.

Hardware-attached actions require explicit user intent for that run:

- flashing firmware
- starting ST-Link GDB server
- attaching GDB to a live target
- resetting the MCU
- resuming target execution

If a run depends on physical state, cable connection, target power, battery simulator setup, debugger connection, or user-controlled instrument state, the agent must not assume it is ready.

---

## Build Rule

Approved build command:

```powershell
cmake --build --preset Debug --verbose
```

Requirements:

- hard timeout
- stdout/stderr capture
- elapsed time report
- exit code report
- process tree kill on timeout
- post-run leftover process check

Check for leftover processes matching:

- `cmake`
- `ninja`
- compiler/linker processes

Known HW4 proof:

- command returned exit code `0`
- output included `ninja: no work to do.`

This proof shows the automation pattern can complete. It does not prove active-target firmware behavior.

---

## Flash Rule

Flash must use the project ELF produced by the approved active-target Debug preset.

No HW6 ELF is approved yet. The safe repository-contained HW6 arrival target is a blocker in [[HW6_Hardware_Documentation_Readiness]]. The imported full-intent HW6 IOC is design input and must not be treated as a first-power image.

The retired HW5 example path was:

```text
firmware/peepshow_hw5_fw0/build/Debug/peepshow_hw5_fw0.elf
```

The HW4-proven ELF path was `build/Debug/PeepShowV0.elf`. Neither that path nor the HW5 example above is valid for HW6.

Approved flash command shape:

```powershell
STM32_Programmer_CLI.exe -c port=SWD freq=4000 -w <approved-hw6-debug-elf> -v -rst
```

Rules:

- do not omit `-rst`.
- use a hard timeout.
- capture stdout/stderr.
- report elapsed time and exit code.
- fail the run if verify fails.
- fail the run if reset is missing or fails.

Known HW4 proof:

- flash exited `0`
- output included `Download verified successfully`
- output included `MCU Reset`
- output included `Software reset is performed`

Important correction:

- a prior autonomous run omitted `-rst` and landed in `HardFault_Handler`.
- that was a bad flash sequence, not normal firmware behavior.
- always include `-rst`.

---

## Debug Rule

Use ST-Link GDB server plus `arm-none-eabi-gdb` with the active Debug ELF symbols.

Safe debug pattern:

1. start `ST-LINK_gdbserver.exe` on a known port.
2. wait briefly for server startup.
3. run `arm-none-eabi-gdb` in `--batch`.
4. attach using `target remote`.
5. read registers/status.
6. source project root `debug.gdb`.
7. run only safe non-resume helpers.
8. detach and quit.
9. kill/cleanup the GDB server.
10. check for leftover GDB/ST-Link processes.

Requirements:

- bounded GDB server startup timeout
- bounded GDB command timeout
- stdout/stderr capture
- process tree cleanup
- no interactive prompt dependency
- no automatic resume unless explicitly approved and externally timed

Safe operations:

```gdb
info registers pc lr sp
info breakpoints
source debug.gdb
ps_mode_flags
ps_power_flags
detach
quit
```

Safe helpers proven on HW4:

- `ps_mode_flags`
- `ps_power_flags`

Known HW4 proof after flash/reset:

- unattended flash plus GDB attach worked
- no user wake/reset was needed
- PC was in `__tx_ts_wait`
- `egMode = 0x00000002`
- `egPower = 0x00000008`
- no HardFault
- no leftover GDB/ST-Link processes

This proof validates the bounded debug method. It does not mark active-target behavior known-good.

---

## Forbidden Automatic GDB Operations

Do not run these automatically unless the user explicitly approves the scenario and the wrapper has an external timeout:

```gdb
continue
run
finish
step
next
until
```

Also forbidden by default:

- helpers ending in `_wait`
- helpers that set `tbreak` then call `continue`
- helpers whose help/comments say `Ctrl-C`
- helpers whose help/comments say `run target`
- long-running trace capture helpers
- breakpoint-heavy flows in high-frequency IRQ or USB paths

Reason:

These operations can block forever or change target timing/state in ways that corrupt evidence.

---

## Leftover Process Checks

After a bounded run, check for leftover processes relevant to the run.

Build:

- `cmake`
- `ninja`
- compiler/linker processes

Flash:

- `STM32_Programmer_CLI`

Debug:

- `ST-LINK_gdbserver`
- `arm-none-eabi-gdb`
- related GDB server helper processes

If leftovers exist:

- terminate only processes started by the wrapper where possible.
- report any process that could not be safely attributed.
- do not kill unrelated user debug sessions unless the user explicitly asked for cleanup.

---

## Evidence Requirements

Build/flash/debug automation used as bring-up evidence must record:

- command line or wrapper invocation
- wrapper version or script path
- timeout value
- elapsed time
- exit code
- stdout/stderr artifact path
- ELF path and firmware commit
- board revision
- debugger/probe model
- target power source
- whether reset was performed
- GDB helper names executed
- leftover process check result

When the run changes hardware state, link the evidence from [[Brought_Up_Tracker]].

---

## Rule

Autonomous build, flash, and debugger inspection are allowed only when bounded, logged, and cleaned up.

Unbounded interactive firmware tools are not allowed.
