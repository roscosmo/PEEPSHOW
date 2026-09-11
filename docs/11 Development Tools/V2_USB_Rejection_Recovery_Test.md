# V2 USB Rejection And Recovery Test

Status: preparation tool and native validator checks pass; device test pending.
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
writes. Shell recovery and installed-generation preservation remain pending on
the device.
