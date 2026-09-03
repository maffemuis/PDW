# End-to-end preservation smoke test

The first public end-to-end fixture is intentionally synthetic silence, not received radio traffic.

`fixtures/silence-44100-mono8.wav` is a generated mono 8-bit PCM WAV containing 1024 zero-level samples at 44.1 kHz. The expected golden output is the empty `golden/silence.jsonl` file.

This scenario verifies the complete preservation path without asserting any real pager content:

1. PDW starts with its default paging decoder mode.
2. `Start_Capturing()` detects explicit preservation replay mode and does not open live WinMM audio.
3. The WAV format and sample rate are validated.
4. Samples are sent through the legacy `Audio_To_Bits()` path.
5. No decoder message is expected from silence.
6. One-shot replay posts a clean application exit.
7. `pdw_preservation_compare` verifies that the produced golden JSONL is exactly empty.

CI enforces a 30-second process timeout so a modal dialog or broken auto-exit cannot hang the workflow indefinitely.

This is only a smoke gate. It does not prove correct POCSAG/FLEX decoding. The next meaningful fixtures must contain explicitly safe synthetic protocol traffic with reviewed expected messages.
