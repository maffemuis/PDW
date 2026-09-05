# PDW v3.3.0-alpha1 — 60 minute exact-head soak evidence

Validation target: `modernization/filter-editor-usability`

Validated SHA: `d923575bffaf72ba626950c5b9990a1450cb1b65`

GitHub Actions run: `33948336663` (`PDW 60m exact-head soak`)

Result: **PASS**

## Wall-clock soak

- Recorded start: `2026-09-05T05:52:52.9824860Z`
- Recorded end: `2026-09-05T06:54:00.1959168Z`
- Decoder soak elapsed: `3603.587 s`
- Resource/liveness samples: `59`
- Raw-log/filter lifecycle cycles: `59`
- Same PDW process remained alive until clean exit with exit code 0.

## Determinism and workload

The exact-head Release build replayed a deterministic synthetic mixed POCSAG/P2000 fixture containing 512, 1200 and 2400 baud traffic, back-to-back bursts, duplicates and multiple capcodes. The fixture was regenerated twice and had the same SHA-256 both times.

- Fixture SHA-256: `9c8fe8312255c950c3faca0a8da483d9850bb99e26de53017775d287b0bd68a4`
- Deterministic decoded-message output SHA-256: `EE0DA26EE16508542135F0D47B773BBA906E2962633DA364785BBA69B3BF88A9`

## Resources

- Private bytes initial: `2,957,312`
- Private bytes maximum: `2,957,312`
- Private-byte growth: `0`
- Working set initial: `7,606,272`
- Working set maximum: `7,647,232`
- Working-set growth: `40,960 bytes`
- Handles initial: `131`
- Handles maximum: `132`
- Handle growth: `1`

No runaway memory or handle growth was observed.

## Repeated safety gates during soak

On every minute-scale lifecycle cycle the run re-executed:

- RX diagnostics/raw-log lifecycle regression;
- raw RX log replay/import regression;
- Multiple Edit preserve/override regression.

All 59 cycles passed. Raw logging remained bounded by the configured `16,777,216` byte limit and the diagnostics test exercised start/write/stop/flush behavior.

## Regression suite

The complete Win32/Release CTest suite ran before and after the soak. Both passes were 21/21 green, including the P2000 preservation cases:

- `pdw_preservation_p2000_short_alpha`
- `pdw_preservation_p2000_short_alpha_divisor`
- `pdw_preservation_p2000_bad_address`

## Portable artifact

The soak run produced the portable Release package and uploaded the evidence bundle as GitHub Actions artifact:

- Artifact: `PDW-60m-soak-evidence-d923575bffaf72ba626950c5b9990a1450cb1b65`
- Artifact ID: `9964833087`
- Artifact ZIP SHA-256: `8e8cb18c5d369ac08b66699a18642d705b02adcce22039fce5646ec2a6197e8e`

The bundle contains `PDW.exe`, its SHA-256 file, License, Readme, deterministic replay captures, stress fixture, resource CSV and exact-head summary.

This evidence applies only to SHA `d923575bffaf72ba626950c5b9990a1450cb1b65`; later commits require their own applicable validation for changed behavior.
