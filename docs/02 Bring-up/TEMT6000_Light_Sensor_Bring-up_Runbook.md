# TEMT6000 Light Sensor Bring-up Runbook

This runbook records the measured HW5 procedure for the ambient light sensor path.

> [!warning] Retired hardware only
> The TEMT6000, `PHOT_EN`, and `PHOT_ADC` paths are absent from HW6. This runbook is not part of HW6 arrival or revalidation and must not be applied to the HW6 IOC. Its results remain historical evidence in [[HW5_Brought_Up_Tracker]].

Related:

- [[Light_Sensor_Contract]]
- [[HW5_Power_Rails]]
- [[Brought_Up_Tracker]]

---

## Scope

This runbook covers:

- `TEMT6000X01` analog light sensor
- `PC2` `PHOT_EN` sensor enable
- `PC3` `PHOT_ADC` / `ADC1_IN4` sampling
- one-shot sampling
- low-rate periodic sampling
- bounded streaming for realtime interactions
- normalized `0-100` output and bands
- dark/covered and bright/exposed response

---

## CubeMX Baseline

Current `.ioc` baseline:

- `PC2` / `PHOT_EN` is regular `GPIO_Output`, push-pull, default low
- `PC3` / `PHOT_ADC` is `ADC1_IN4`
- `ADC1.NbrOfConversion = 1`
- `ADC1_IN4` regular conversion uses `ADC_SAMPLETIME_814CYCLES`
- ADC interrupt is not enabled for first bring-up
- ADC DMA is not enabled for first bring-up
- no hardware light-threshold wake pin exists

`PHOT_EN` is active high:

- low: TEMT6000 circuit unpowered/off
- high: TEMT6000 circuit powered/on

Firmware safe default is low/off.

---

## Baseline State Sequence

Use this sequence for first bring-up:

1. Boot with `PHOT_EN` low/off.
2. Read ADC once while disabled and record the off-state value.
3. Assert `PHOT_EN`.
4. Wait a measured settle interval.
5. Take a bounded ADC sample burst.
6. Deassert `PHOT_EN`.
7. Repeat in covered/dark, room-light, and bright-safe conditions.
8. Compute preliminary normalization points.
9. Validate one-shot sampling.
10. Validate low-rate periodic sampling where the sensor powers down between bursts.
11. Validate bounded streaming mode for active gameplay-style feedback.

First bring-up may use polled ADC conversion. Do not add ADC DMA or interrupt complexity until sampling cadence or CPU measurements justify it.

---

## Command / Configuration Ledger

Record exact configuration and measured values here once HW5 is available.

| Step | Operation | Peripheral / Pin | Value | Delay | Expected Result | Notes |
|---|---|---|---|---|---|---|
| 1 | safe default check | `PC2` / `PHOT_EN` | low | N/A | sensor circuit off | record ADC off-state if useful |
| 2 | ADC baseline read | `ADC1_IN4` | polled conversion | N/A | bounded value | expected value depends on circuit |
| 3 | sensor enable | `PC2` / `PHOT_EN` | high | measured settle | sensor circuit on | settle becomes `KNOB_SENSOR_LIGHT_SETTLE_MS` |
| 4 | dark sample | `ADC1_IN4` | sample burst | after settle | high/dark ADC value | cover sensor physically |
| 5 | room sample | `ADC1_IN4` | sample burst | after settle | mid/high ADC value | normal indoor condition |
| 6 | bright sample | `ADC1_IN4` | sample burst | after settle | low ADC value without unsafe exposure | avoid overdriving assumptions |
| 7 | one-shot validation | state machine | enable/sample/off | bounded | snapshot valid | raw ADC diagnostics only |
| 8 | periodic validation | state machine | low-rate cadence | configured period | powers down between bursts | used by system/display policy |
| 9 | streaming validation | state machine | requested sample rate | bounded lease | responsive normalized value | expires and powers off |
| 10 | fault validation | ADC/sample policy | stuck/rail/impossible calibration if practical | N/A | `LIGHT_ERROR` or invalid snapshot | non-critical fault |

Measured HW5 note: the Phase 4 `fw0` polled probe showed inverse polarity for this circuit. Brighter light drives the ADC count lower.

---

## Calibration Outputs

Record calibration decisions here before implementation freeze:

- dark/covered ADC value: covered sample observed around `0x3F9F..0x3FAB`
- expected indoor ADC range: ambient sample observed around `0x38AF..0x3995`
- bright-safe ADC value: direct light sample observed down to `0x0011`
- raw ADC value treated as saturated
- raw ADC value treated as sensor/circuit fault
- mapping used for `normalized_0_100`
- band thresholds for `DARK`, `DIM`, `NORMAL`, `BRIGHT`, and `SATURATED`
- filtering/sample-count policy
- maximum approved streaming rate
- maximum approved streaming lease duration

The Reference Game and Engine consume normalized values and bands only. Raw ADC is calibration/diagnostics.

---

## Validation Procedure

1. Confirm `PHOT_EN` active-high behavior: low powers sensor circuit off, high powers TEMT6000 circuit on.
2. Confirm ADC reading while sensor disabled.
3. Enable sensor and measure settle time.
4. Capture dark/covered ADC value.
5. Capture room-light ADC value.
6. Capture bright-light ADC value within safe sensor limits.
7. Validate one-shot sample path.
8. Validate periodic sample path.
9. Validate bounded streaming path at a requested sample rate.
10. Validate normalized `0-100` output and light-band classification.
11. Validate fault behavior for stuck-at-rail or impossible calibration if practical.
12. Confirm quiesce/sleep transitions leave `PHOT_EN` low/off.

---

## Evidence Requirements

Record in [[Brought_Up_Tracker]]:

- active-high enable observation
- settle time observation
- raw ADC samples for dark/room/bright conditions
- normalized output examples
- periodic/streaming timing result
- fault behavior notes if tested

The game-facing API uses normalized values; raw ADC is diagnostic/calibration only.
