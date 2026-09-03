# PDW preservation capture

This directory establishes a behavior-preservation contract before the legacy decoder is refactored.

## Capture point

Direct decoder-originated calls to `ShowMessage()` pass through an observation wrapper. The wrapper copies the current legacy message fields and the active `message_buffer`, optionally appends one JSON object per line, and then calls the original `ShowMessage()` implementation.

The capture therefore happens **before** the legacy `ShowMessage()` filter, duplicate-blocking, UI, logging and language-remapping behavior. Calls that `Misc.cpp` makes internally while transforming group calls remain inside the legacy implementation and are deliberately not intercepted.

Capture is disabled by default. If `PDW_PRESERVATION_CAPTURE` is unset or empty, no capture file is created.

Example on Windows PowerShell:

```powershell
New-Item -ItemType Directory -Force preservation-local | Out-Null
$env:PDW_PRESERVATION_CAPTURE = "$PWD\preservation-local\baseline.jsonl"
.\build\Release\PDW.exe
Remove-Item Env:PDW_PRESERVATION_CAPTURE
```

## JSONL contract

Schema `pdw-preservation-v1` currently records:

- `capture_point`
- `capcode`
- `time`
- `date`
- `mode`
- `type`
- `bitrate`
- `message`
- `mobitex`

Legacy bytes outside printable ASCII are escaped as `\u00XX`. This keeps the JSON valid while retaining the exact byte value for future comparisons.

## Privacy and test corpus policy

Do **not** commit real received pager traffic or other potentially private radio traffic to this public repository. Local captures belong under `preservation-local/` or `tests/preservation/private/`, both of which are ignored by Git.

A future public regression corpus must use synthetic, anonymized or otherwise explicitly safe samples.

## Next step

This foundation does not yet replay recorded audio automatically. The next preservation step is an offline recording-input adapter that feeds the existing audio-to-bits/decoder path and compares its emissions against reviewed golden JSONL fixtures.
