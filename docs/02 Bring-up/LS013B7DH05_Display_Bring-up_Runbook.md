# LS013B7DH05 Display Bring-up Runbook

This runbook adapts the measured HW5 Sharp Memory LCD procedure for active HW6 revalidation.

> [!important] HW6 reuse
> HW6 hardwires the display translator enabled so EXTCOMIN always passes. Ignore the historical HW5 `PD2` / `VLT_LCD` control steps, use the HW6 pin/DMA/power contracts, and record all new evidence in [[HW6_Brought_Up_Tracker]]; no HW5 pass transfers to HW6.

Related:

- [[Display_and_Rendering_Contract]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_DMA_Map]]
- [[HW6_Power_Rails]]
- [[HW6_Brought_Up_Tracker]]

---

## Scope

This runbook covers:

- `LS013B7DH05` panel validation
- logical landscape `168 x 144` mapping from native portrait `144 x 168`
- `SPI3` TX-only transfer path
- `PA15` display chip select behavior
- hardwired-enabled display level-translator behavior
- `PC13` `LCD_1HZ` RTC calibration output for EXTCOMIN/VCOM
- full-frame updates
- partial updates
- SRAM4 DMA-safe display buffer placement
- low-power static hold behavior
- LPBAM display experiment evidence after baseline display validation

---

## CubeMX / Electrical Baseline

| Function | MCU resource | Required baseline |
| --- | --- | --- |
| Display bus | `SPI3` TX-only master | `PC10` SCK, `PC12` MOSI, `PA15` NSS |
| Display DMA | `LPDMA1_CH0`, `LPDMA1_REQUEST_SPI3_TX` | memory-to-peripheral display payload transfer |
| Level translator | hardwired TXU0104RUTR enable | continuously enabled on HW6; no MCU OE GPIO |
| EXTCOMIN / VCOM | `PC13` `LCD_1HZ` | RTC 1 Hz calibration output |

The HW6 translator path is continuously enabled while the display rail is powered. Firmware must keep SPI idle until `thDisplay` owns it and must establish and maintain `LCD_1HZ`; it must not create a software translator-enable sequence.

The display has no readback path. Every bring-up claim must come from waveform evidence, known pattern photos, framebuffer/payload logs, or current measurements.

---

## Baseline State Sequence

This sequence proves the normal renderer path before any LPBAM experiment.

1. Boot with SPI3 idle; confirm no firmware attempts to control a `VLT_LCD` GPIO.
2. Start the display owner and establish `LCD_1HZ`.
3. Confirm `LCD_1HZ` is present at the panel side of the always-enabled level translator.
4. Send a full clear frame at conservative SPI speed.
5. Send full-frame black, white, checkerboard, border, vertical-line, and horizontal-line patterns.
6. Confirm logical landscape `168 x 144` maps correctly onto native portrait `144 x 168`.
7. Confirm pixel polarity, row order, byte order, and line address format.
8. Perform a partial update on a single line or small row range.
9. Perform a dirty-region update from framebuffer change tracking.
10. Confirm full-frame fallback triggers when dirty coverage exceeds the chosen threshold.
11. Confirm display payload DMA reads from the approved SRAM4 display buffer region.
12. Enter low-power/static hold with the image visible and `LCD_1HZ` maintained.
13. Resume and perform another partial update without stale transfer state.

If any of these fail, LPBAM display animation remains out of scope until the normal path is stable.

---

## Pattern / Mapping Ledger

Populate this table during HW6 bring-up. The current values remain placeholders until measured on an identified HW6 unit.

| Test pattern | Purpose | Expected evidence | Measured result | Status |
| --- | --- | --- | --- | --- |
| all white | pixel polarity and clear path | uniform cleared display | TBD | open |
| all black | pixel polarity and full-frame fill | uniform filled display | TBD | open |
| checkerboard | byte order and adjacent pixel mapping | alternating pattern with no skew | TBD | open |
| border | logical edge mapping | visible border on all four logical edges | TBD | open |
| single pixel | coordinate transform | expected logical coordinate appears | TBD | open |
| vertical line | x-axis mapping | straight vertical line in landscape space | TBD | open |
| horizontal line | y-axis mapping | straight horizontal line in landscape space | TBD | open |
| small dirty row range | partial update | only target rows change | TBD | open |
| SRAM4 DMA source | buffer placement and DMA reachability | payload read from approved SRAM4 display buffer | TBD | open |
| static hold | low-power hold behavior | image remains visible; EXTCOMIN remains valid | TBD | open |

---

## LPBAM Experiment Gate

LPBAM has a measured HW5 baseline but remains an HW6 revalidation experiment rather than the baseline renderer. Attempt it after the normal HW6 display sequence passes and before final sleep/wake integration is closed.

Required LPBAM experiment flow:

1. Build a tiny prevalidated sequence of two or more static idle-animation frames.
2. Convert the sequence into bounded SPI3/LPDMA/LPBAM payloads using the same verified row format as the normal renderer.
3. Place the sequence payloads in the approved SRAM4 display/LPBAM buffer region.
4. Confirm the scenario can run for a fixed window without CPU intervention.
5. Trigger wake/exit through input or RTC policy and prove `thDisplay` reclaims SPI3/LPDMA/LPBAM ownership.
6. Compare current draw against the normal RTC wake/partial-update idle strategy.
7. Confirm the continuously enabled translator carries EXTCOMIN throughout the scenario.

Acceptance requires correct image output, clean ownership reclaim, correct EXTCOMIN behavior, and measured current benefit or a clearly documented reason to keep the path disabled.

---

## Command / Configuration Ledger

| Step | Configuration | Expected result | Measured result | Status |
| --- | --- | --- | --- | --- |
| safe boot | SPI3 idle; no translator GPIO | no unintended display transfer | TBD | open |
| translator path | hardwired enabled | SPI and EXTCOMIN path continuously available | TBD | open |
| EXTCOMIN | RTC 1 Hz calibration output | `LCD_1HZ` reaches panel | TBD | open |
| SPI baseline | conservative SPI3 TX | valid SCK/MOSI/NSS waveforms | TBD | open |
| full frame | clear/fill patterns | correct full-screen image | TBD | open |
| orientation | mapping patterns | logical `168 x 144` landscape confirmed | TBD | open |
| partial update | row/range payload | only target rows change | TBD | open |
| SRAM4 display buffer | approved SRAM4 DMA source | LPDMA reads correct payload without corruption/fault | TBD | open |
| static hold | low-power hold | image remains visible | TBD | open |
| suspend/resume | quiesce then update | no stale DMA/SPI state | TBD | open |
| LPBAM experiment | prevalidated SRAM4 animation payload | autonomous sequence and clean exit | TBD | open |
| fault injection | transfer/init fault | fatal display fault path entered | TBD | open |

---

## Validation Procedure

1. Confirm the HW6 translator is hardwired enabled and no MCU GPIO controls it.
2. Confirm RTC 1 Hz calibration output appears on `LCD_1HZ` when display policy enables it.
3. Confirm SPI3 SCK/MOSI/NSS waveforms at bring-up speed.
4. Send known full-frame test pattern.
5. Validate logical landscape orientation and row/column mapping.
6. Validate clear, checkerboard, border, and single-pixel/line patterns.
7. Validate partial update of a small dirty rectangle or line range.
8. Validate display payload transfer from the approved SRAM4 display buffer region.
9. Validate static hold after MCU idle/sleep entry with the hardwired translator path and `LCD_1HZ` maintained.
10. Validate fault behavior when display init/transfer fails.
11. Attempt the LPBAM idle-animation experiment after full/partial/static-hold validation and before final sleep/wake integration closure.
12. Confirm no active, sleep, or LPBAM path attempts translator duty cycling.

---

## Evidence Requirements

Record in [[HW6_Brought_Up_Tracker]]:

- logic capture or scope notes for SPI3 and EXTCOMIN
- photos of known display patterns
- orientation verification notes
- partial-update verification notes
- SRAM4 display buffer DMA reachability evidence
- low-power static-hold observation
- measured current for display hold if available
- LPBAM evidence: prevalidated SRAM4 sequence correctness, autonomous transfer window, wake/exit behavior, ownership reclaim, image correctness, and current comparison

Do not mark LPBAM display animation supported without measured scenario evidence. If it fails, normal display bring-up may still pass, but LPBAM remains unavailable.
