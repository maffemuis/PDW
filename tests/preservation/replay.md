# Decoder preservation replay

This replay path exists only to preserve current decoder behavior while PDW is modernized.

It is **opt-in** and does not replace normal WinMM audio capture unless `PDW_PRESERVATION_REPLAY_WAV` is explicitly set.

## Requirements

The replay recording must be a mono, 8-bit PCM RIFF/WAVE file. Its sample rate must exactly match PDW's configured `Profile.audioSampleRate`.

Exactly one decoder mode must be active:

- POCSAG/FLEX paging;
- ACARS;
- MOBITEX;
- ERMES.

`PDW_PRESERVATION_CAPTURE` must also point to a JSONL output file. Replay refuses to run without capture enabled so a preservation run cannot silently discard its decoder output.

## PowerShell example

```powershell
New-Item -ItemType Directory -Force preservation-local | Out-Null
Remove-Item .\preservation-local\baseline.jsonl -ErrorAction SilentlyContinue

$env:PDW_PRESERVATION_REPLAY_WAV = "C:\captures\safe-test.wav"
$env:PDW_PRESERVATION_CAPTURE = "$PWD\preservation-local\baseline.jsonl"

.\build\Release\PDW.exe

Remove-Item Env:PDW_PRESERVATION_REPLAY_WAV
Remove-Item Env:PDW_PRESERVATION_CAPTURE
```

When replay mode is requested, PDW probes and validates the WAV before any sample is sent into the decoder. On validation failure, live audio is not started as a fallback; the run fails closed.

## Routing

Replay uses the same legacy functions as live audio:

- paging -> `Audio_To_Bits()`;
- ACARS -> `ACARS_To_Bits()`;
- MOBITEX -> `MOBITEX_To_Bits()`;
- ERMES -> `ERMES_To_Bits()`.

`Reset_ATB()` is called immediately before replay, mirroring the reset performed by normal audio startup.

## Privacy

Do not add real private received traffic to this public repository. Keep local recordings and captured JSONL under ignored local paths. Public golden fixtures must be synthetic, anonymized, or otherwise explicitly safe to publish.
