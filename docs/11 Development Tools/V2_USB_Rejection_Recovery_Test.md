# V2 USB Rejection And Recovery Test

Status: device rejection, shell recovery and preservation of the installed game
passed. The corrected checksum-reason reporting retry passed on 2026-09-12;
the final good-egg recovery reinstall is still pending.
Run after the unchanged fixture has passed the normal Studio export/install test.
Keep the same firmware baseline. No rebuild, reflash or new GUI capability is
needed. Authority: [[Storage_and_Installer_Contract]].

## What This Checks

A damaged staging egg must be rejected before it is offered as VALID or written
to the active package area. The shell must remain usable, the existing installed
generation must survive, and the original good egg must still be installable.

This test damages one bit of the stored SHA-256 digest only. Header, chunk CRCs,
payload, file length and authored content remain identical to the good export.
It tests integrity rejection and recovery, not semantic rejection with valid
checksums, failed installed-package boot or power loss during flash writes.
Do not interrupt power during this sequence.

## Prepare From The Actual Studio Egg

Once GUI provides the good exported egg path, use it as `$goodEgg` below. Use a
new local output directory, not the device's MSC drive:

```powershell
$goodEgg = 'REPLACE_WITH_ACTUAL_STUDIO_EXPORT_PATH.egg'
python tools/authoring/prepare_v2_rejection.py --egg "$goodEgg" --output-dir firmware/peepshow_hw6_fw0/build/v2_usb_rejection_01
```

The command validates the original against the public restricted V2 profile,
then creates `DO_NOT_INSTALL_bad_digest.egg` and `evidence.json`. It never changes
the source egg, refuses an existing output directory, and does not access a
device. Use a new numbered directory for a later run. The evidence file records
both full-file hashes, the one-byte mutation and hardware status `not_run`.
Generating the file is not a hardware pass. Do not rebuild/export the damaged
copy through Studio; that would no longer test these exact bytes.

## Device Sequence

1. Confirm the good Studio-exported fixture is installed: digits continue through
   A/B changes, the bottom square appears once, and HOLD START opens the shell.
   After work is finished, halt and record the baseline generation, size,
   selected record, install count and preflight count using the block below.
   Resume before performing any on-device action.
2. Open MSC through Packages. On the host, remove other `.egg` files from the
   staging drive only, then copy `DO_NOT_INSTALL_bad_digest.egg` as its sole egg.
   This is deliberate bad staging data, not a request to install it. Do not
   copy `evidence.json` or touch the installed flash/index through GDB.
3. Eject safely, reclaim MSC on-device and request package scan. Expect busy
   feedback followed by a recoverable package error, never VALID/INSTALLED.
   Wait for completion before halting and collect the same block immediately,
   before another shell action replaces the workflow timing record.
4. Resume. B must return from the package error to the package controls. Check
   shell navigation with joystick and buttons, and the ability to enter MSC
   again. Deliberately reboot after the failed scan: the previously installed
   good V2 scene must still launch and behave normally. Print again after boot.
5. Return to MSC and replace the bad staged egg with the original good Studio
   egg as the only candidate. Eject, reclaim, scan, install, PLAY and deliberately
   reboot. Expect successful installation and the same scene behavior. This last
   step intentionally reinstalls the good package and advances its generation.

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_workflow_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_persistent_install_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_index_scan_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_installed_prints.gdb
```

If STOP2 disconnects GDB, reconnect without reset for the post-scan snapshot.
Only the explicit reboot steps count as boot tests. Avoid halting during scan,
installation, playback handoff or audio; debugger timing is not natural timing.

## Reading The Result

- Failed scan: preflight count advances, status is nonzero, reason is `4`
  (DIGEST), and pending/reserved returns to zero after completion.
- The completed workflow describes SCAN (`4`), ERROR (`15`), active `0` and
  nonzero status. No erase/write/verify/commit phase should be visited for this
  rejected scan. A busy thread or request counter alone does not prove rejection.
- Compared with the baseline, install count and selected installed generation,
  record, slot, address and size must not change before the good recovery install.
  Old successful install status/stage fields can remain visible: they describe
  an earlier operation, not installation of the bad egg. Zero unused writer
  fields are not evidence that a write completed successfully.
- The index print is the last recorded scan, not a new flash read. Deliberate
  reboot and successful launch of the old generation supply the persistence
  check. Boot resets session counters; compare identity/generation, not counts
  across the reset.
- After recovery installation, require status `0`, stage `11`, zero actual
  verify mismatches, a newer VALID generation, and source/model `3/2` on PLAY
  and reboot. A/B continuity and timer reveal must be visibly confirmed.

If the bad egg is offered as VALID, the shell stops responding, or the installed
generation changes during rejection, stop and retain the debugger evidence.
Do not hide that result by reinstalling or reflashing first. No debugger enable
or embedded-install helper is part of this test.

## Local Verification

Five preparation tests cover deterministic one-bit damage, recorded hashes,
source preservation, overwrite refusal and invalid/V1 input rejection. Both
native V2 profile tests consume the preparation tool's exact output, report
loader reason `4`, preserve the live V1/V2 context and accept a good candidate
after the failures. These native checks do not exercise USB, the panel or NOR
writes. The subsequent device recovery result and separate reporting correction
are recorded below.

## Device Recovery Pass And Reporting Correction

The user supplied the negative-workflow capture and confirmed B returns to the
shell and deliberate reboot returns to the working 1-2-3-4/A-B scene.
Preflight count/status was 1/1, reservation 0, terminal workflow ERROR, active 0.
The writer remained at install count 0 and stage IDLE, with no package writes.
The index retained the 2196-byte VALID generation 6 at `0xc0000`; journal
generation 5 remained PENDING. No erase/write/verify/commit workflow phase was
recorded. Those results establish rejection before replacement and recovery,
not just successful queue dispatch.

The workflow reported reason 11 (RENDER), rather than the intended checksum
reason 4. Investigation found `PS_HW6_RTOS_HandleRuntimeCommand` unconditionally
relabelled every V2 preflight failure as RENDER and hardcoded scene 1. Thus the
old capture does not establish the exact rejection stage. The local damaged
artifact changes only the final digest bit; its SHA-256 is
`53f20ca33cb1c7b1443ddfdec34a4d7d7dd469384b999eba6dbc0015c48bf6ad`.

The scoped correction moves the existing validation/reporting block into
`PS_HW6_RTOS_RunPackageValidation` and preserves the current candidate's loader
reason, graph/waiting failure or actual projection/raster failure. It requires a
fresh request token before using candidate details. A refused/busy/transport
failure with no specific content rejection reports reason NOT_RUN, not a false
RENDER or stale checksum error. Scene 0 means not identified. Existing probe
layouts, admission decisions, queue/lease handling, writes and clock policy are
unchanged. The ARM stack check includes the new direct call in its preflight
prefix rather than omitting that frame.

Native owner-queue coverage exercises the actual validation/reporting function:
good V2, one-bit digest and header-CRC damage, oversized input, clock refusal,
real missing-sprite raster rejection, display busy, timeout, stale-detail
refusal, late completion and subsequent good validation. The live scene and
catalog remain unchanged. This is local proof, not the firmware retry result.

Verification: all 321 host/native tests and the Debug firmware build pass.
The scoped ARM stack check, including the new call frame, passes at a maximum
1808 C bytes plus 512 reserve within the unchanged 4096-byte runtime stack.
The preflight path itself uses 1648 C bytes plus reserve. Linked RAM remains
550936 bytes, SRAM4 15480 bytes, and ROM is 868424 bytes. These are build and
compiler measurements, not a whole-firmware worst-case stack proof.

For the retry, flash the corrected firmware once; this is a reporting change,
not a request to reinstall the good package. Rescan the same bad staged egg,
then use the existing workflow helper after completion. Require status failure,
reason 4, reservation 0, no writer start, and retained generation 6. A wake press
before halting is acceptable. GUI work and its export capabilities are unaffected.
Then finish the good-source recovery reinstall in the sequence above.

## Corrected Digest Reporting Passed (2026-09-12)

The retry reports workflow action INSTALL, phase ERROR, status `0x20`, active 0;
preflight count/status/scene/reason is 1/1/0/4 and reservation is 0. Reason 4 is
the intended digest rejection. Scene 0 correctly indicates that no scene was
identified before the integrity failure. No erase, program, verify or commit
phase was visited. This is a completed rejection, not merely a queued request.

The index scan retains VALID generation 6, selected record 1/slot 0, address
`0xc0000`, size 2196, availability 1 and matching CRCs. The older generation-5
PENDING journal record is unchanged. Earlier user-observed B-to-shell and
reboot-to-good-scene evidence still stands; no new reboot observation was
provided with this reporting retry.

Busy feedback completed in 60 ms. Total recorded workflow time is 9420 ms:
STARTING 60, PREPARING 50, SCANNING 6140, READING 3110 and VALIDATING 60 ms.
SCANNING/READING/VALIDATING phase-entry samples show HCLK 24 MHz and OSPI-policy
readback 128 MHz. These wall times do not identify CPU utilization, actual bus
throughput or the cause of the delay. Latency investigation remains separate;
this result does not justify changing clocks or removing validation checks.

The diagnostic correction and retained-generation checks now pass on hardware.
Next replace the staged bad egg with the original Studio export and complete
the normal scan/install/PLAY sequence without a firmware reflash, to close the
remaining recovery-reinstall check. GUI requires no change for this result.
