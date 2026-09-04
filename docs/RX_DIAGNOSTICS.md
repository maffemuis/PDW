# PDW RX diagnostics

The modernization build exposes live receive diagnostics in **Help > Debug Information (F12)** for troubleshooting the Serial/RS232 POCSAG path.

The diagnostics are observational only: they do not alter decoder thresholds, sync tolerance, polarity, filtering, or message handling.

## Live fields

The RX/COM section reports:

- COM open/closed state and selected COM number;
- the DCB read back from Windows after `SetCommState` (baud, data bits, parity, stop bits and flow flags);
- cumulative read, byte and symbol activity plus age of the last receive;
- Windows serial error counters (read, framing, parity, overrun and receive-overflow);
- current 512/1200/2400 POCSAG preamble counters and rate-detection totals;
- current POCSAG rate/active state and configured invert setting;
- sync search/lock state, sync found/lost counters and nearest observed sync distance;
- codeword totals split into clean, corrected and uncorrectable results;
- current Raw RX Log state, byte count, size limit and path.

`Copy diagnostics` copies the current snapshot as plain text so it can be attached to a fault report.

## Raw RX Log

`Start Raw RX Log` is explicitly opt-in. It creates a timestamped `PDW-RX-*.log` file in the PDW working directory. The log records relative timestamps, raw COM bytes in hexadecimal, the corresponding MSB-to-LSB bit representation and decoder state events such as rate detection, sync and codeword correction status.

The raw log deliberately excludes SMTP credentials and decoded message payload text. It has a hard default limit of **16 MiB**. Reaching that limit stops logging automatically. `Stop Raw RX Log` flushes and closes the file; PDW also closes an active raw log during application shutdown.

## Reading a no-message capture

For a receiver that shows RF activity but no decoded message, use the fields in this order:

1. `COM OPEN` and increasing `Traffic: bytes` proves the Windows COM receive path is active.
2. Increasing POCSAG preamble counters and a 512/1200/2400 detection proves the timing detector is recognizing a candidate POCSAG stream.
3. `Sync: LOCKED` or a low sync distance proves the decoder sees a valid or near-valid POCSAG sync word.
4. Codeword counters distinguish a clean/correctable stream from an uncorrectable bitstream.
5. If codewords are valid but `Messages` remains zero, investigation moves further downstream into address/message assembly rather than the physical COM path.

A Raw RX Log can be replayed/analyzed later without guessing which stage of the receive chain failed.
