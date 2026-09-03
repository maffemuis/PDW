# Golden preservation output

PDW preservation uses two complementary JSONL outputs:

- `PDW_PRESERVATION_CAPTURE` keeps the complete observation, including time and date, for diagnosis.
- `PDW_PRESERVATION_GOLDEN_CAPTURE` writes only stable decoder-behavior fields for regression comparison.

The deterministic golden schema is `pdw-golden-v1` and contains:

- capture point;
- capcode/address;
- mode/protocol;
- message type;
- bitrate;
- message payload;
- MOBITEX payload field.

Time and date are deliberately excluded because they describe when a replay was executed, not how the decoder interpreted the recording.

## Golden comparison

`pdw_preservation_compare` performs an exact binary comparison and reports the first differing JSONL line. Exact comparison is intentional: once a fixture is reviewed, any decoder-visible change must be explicit.

Example:

```powershell
.\build\Release\pdw_preservation_compare.exe `
  .\tests\preservation\golden\pocsag-example.jsonl `
  .\preservation-local\actual.jsonl
```

A mismatch, additional/missing message, changed byte escaping, changed ordering, or missing final newline all fail the comparison.

## Fixture policy

Do not commit private received radio traffic. Public recording/golden pairs must be synthetic, anonymized, or otherwise explicitly safe to publish.
