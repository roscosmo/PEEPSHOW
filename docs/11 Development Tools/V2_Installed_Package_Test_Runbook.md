# Restricted V2 Installed Package Test

Status: hardware boot, reinstallation and launch passed after the runtime stack
correction (2026-09-11). Detailed installed low-power timing/current acceptance
remains separate; ordinary Studio V2 export is still disabled.

Source: `examples/authoring/native_v2_installation.peepproj`, imported unchanged
from GUI commit `e7f11f011fcbd91001aac0d15a85543c6212f3ab`. Its README describes
the earlier GUI-only checkpoint. This is a real V2 egg, not a converted V1 or
linked development scene.

## Artifact

From the workspace root:

```powershell
python tools/authoring/build_object_development.py --project examples/authoring/native_v2_installation.peepproj --egg-output firmware/peepshow_hw6_fw0/build/native_v2_installation.egg
cmake --build firmware/peepshow_hw6_fw0/build/Debug
```

The egg is 2,196 bytes. `--egg-output` leaves both linked firmware fixtures
unchanged. Ordinary Studio export is still disabled. Flash the built firmware
normally; do not run any development scene enable helper for this test.

## USB Install, PLAY, Reboot

1. HOLD START to open the shell. Enter USB MSC through Packages.
2. Copy `native_v2_installation.egg` to the exported drive's root. Keep it the
   only `.egg` candidate there; remove old staged eggs using the host if needed.
   The currently installed package is not replaced until Install is chosen.
3. Eject safely, leave MSC, then use package scan and Install.
4. At INSTALLED / A PLAY, press A. Expect digits 1-2-3-4 at 400 ms per frame,
   fixed B/A slots, a marker that moves once per A/B selection, and a bottom
   square that appears once after two seconds. No audio is authored.
5. Release the controls. Autonomous animation must return without shifting
   static outlines or restarting digits. HOLD START and Resume must work.
6. Wake normally, halt and collect the commands below. Reconnect without reset
   if STOP2 disconnected GDB. A reconnect alone is not a reboot test.
7. Deliberately reset. The same V2 package must launch without any enable helper:
   marker starts in B and the two-second reveal occurs afresh. Print again.

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_workflow_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_persistent_install_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_persistent_runtime_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_installed_prints.gdb
```

The embedded-install helper still installs the linked V1 test package. The
awake/LPBAM development launch helpers still launch ROM diagnostics, not this
installed egg. Do not use either for this acceptance test.

## Evidence

- Install: status 0, stage 11, size 2196, slot 0 at `0xc0000`, zero verify
  mismatches and a committed format-2 VALID journal record.
- PLAY and deliberate reboot: source 3, execution model 2, installed object
  bytes 2196, scene active and activation status 0. Boot install counters may
  be NOT_RUN because boot does not reinstall the package.
- Candidate: matching completed token, lease 0, status/profile/schedule/display
  0. Fields describe the latest check, including an A/B transaction; they are
  not cumulative proof that every possible state fits.
- Timer due/applied/error 1/1/0 after expiry. RTC selection requires sleep before
  expiry; an awake expiry is not a failed timer test.
- LPBAM publish/wake statuses 0, measured/reconciled sleeps equal, observed
  A/B continuity. Snapshots are last runtime projections, not live DMA frames.
  A halted in-flight transfer can legitimately show NOT_RUN.
- Visible behavior proves drawing; dispatch counters alone do not. Precise
  cadence/current need separate measurement, preferably PPK2 without a debugger
  disturbing STOP2. Do not report a current pass from GDB alone.

Restrictions remain one continuous scene, fully resident <=64 KiB, no audio or
scene exits. This does not enable general V2 export, multi-scene packages,
streaming V2 assets or physical power-cut acceptance.

## Installed Launch Stack Regression

The 2196-byte GUI egg installed successfully, but PLAY faulted during nested
V2 admission/decoding. CFSR was `0x00100000` (STKOF), HFSR `0x40000000`, the
current thread was `ps_threads[8]`, and PSP `0x20078cf0` equalled that thread's
stack start. The allocation was 2048 bytes. This is not an egg-validation or
flash-write failure. Stack-limit failure makes the saved exception frame
unreliable; do not claim an exact faulting instruction from that frame.

Correction: runtime stack knob/default is 4096 bytes. Candidate and installed
LPBAM probe resets initialize in place instead of constructing Debug-build
automatic struct temporaries. Candidate-check stack use drops by 256 bytes;
installed-launch initialization drops by 152 bytes. No package bytes, wire
format, peripheral ownership, clocks or sleep policy change.

`tools/authoring/check_v2_runtime_stack.py` uses the configured ARM Debug compile
database and GCC stack/call-graph reports. It checks preflight, command and shell
PLAY launch, input/timer admission, and first presentation. Direct prefix edges
are verified; the two known scene-admission callback edges are explicit.
The final checked C maximum is 1816 bytes, plus a 512-byte exception/library
reserve: 2328 of 4096 bytes, leaving 1768 bytes. Before the in-place reset, that
same launch chain needed 2072 C bytes, already exceeding the old allocation.
These are scoped compiler measurements, not a complete firmware worst-case
proof. Unmeasured library/RTOS/assembly leaves are reported. Host-native tests
alone did not model the Cortex-M33 stack limit or the ARM Debug call frames.

```powershell
python tools/authoring/check_v2_runtime_stack.py
```

The additional 2048 bytes come from the existing fixed ThreadX byte pool, not a
new heap or an enlarged linker region. Check RTOS init status 0, runtime stack
size 4096, and remaining pool allocation in the installed helper. Its saved-SP
margin is a context-switch snapshot, not peak stack usage.

For the retry, flash the corrected firmware normally and boot the already
installed V2 egg; no reinstall is needed. Do not run the embedded-install helper
because it would replace it with the linked V1 package. Verify digits, A/B,
timer reveal, shell/Resume and autonomous playback, then capture
`__fw0_object_installed_prints.gdb`. Repeat a deliberate boot. Treat observations
individually; the confirmed retry below does not cover every acceptance item.

Local verification after the correction: 303 host/native tests pass, including
the configured ARM stack-budget check. The full Debug firmware build passes
(existing unused-parameter/function warnings remain). Linked RAM remains
550936 bytes because ThreadX allocations come from its existing fixed pool;
ROM is 868088 bytes and SRAM4 is 15480 bytes. These local checks alone do not
prove hardware launch; the following device result does.

## Hardware Retry: Boot, Reinstall And Launch Passed

On 2026-09-11 the user confirmed boot into the numbered 1-2-3-4/A-B scene,
then a successful reinstall and launch. The latest installed-object helper shows:

- Runtime stack 4096 bytes, RTOS init status 0, pool available before/after
  40952/13544 bytes. Saved SP margin 3276 bytes is not a peak-use measurement.
- Installed source 3, scene active 1, activation status 0, execution model 2,
  resident/index size 2196 bytes. Slot 0, generation 4, address `0xc0000`.
- UI/class/lifecycle 6/2/2, latest admission token/complete 6/6, lease 0,
  status/profile/schedule/display all 0, payload 8 chunks / 4672 bytes.
- Display request/complete 4/4, result 0, lease fault 0. Visible scene plus
  completed display work confirms more than successful queue dispatch.
- Timer due/applied/error 2/2/0, RTC selections 2, reveal flag 1. These counters
  span the reported boot/reinstall session; they do not imply two firings per
  scene activation.
- LPBAM enabled 1, fault 0, publish status 0, 4 steps at 400 ms.

WFI returns/measured/reconciled were 2/2/1 with clock status 0.
`PS_HW6_RTOS_ObjectSleepClockFinish` increments measured sleep count;
`PS_HW6_RTOS_ObjectAdvance` later consumes missing time and updates reconciled
count. A halt between these stages can produce this difference, but this single
snapshot does not establish that cause or demonstrate final reconciliation.
Do not mark a complete low-power timing/current pass from this capture. A settled
post-wake observation is still needed for that accounting check, and physical
cadence/current measurements remain separate.

The launch stack regression is cleared for this fixture's observed boot and
reinstall/launch paths. This is not general V2 export readiness, all-state
coverage, physical power-cut acceptance or unrestricted package support.
