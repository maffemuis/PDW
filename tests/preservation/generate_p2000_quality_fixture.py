#!/usr/bin/env python3
"""Generate deterministic POCSAG message-codeword error fixtures.

These fixtures are synthetic and privacy-safe.  They exercise BCH correction
and fail-closed handling at the real replay/decoder boundary without using
off-air traffic.
"""

from __future__ import annotations

import hashlib
import sys
from pathlib import Path

import generate_p2000_fixture as base

MESSAGE = "BRANDWEER TEST"
ERROR_MASKS = {
    0: 0x00000000,
    1: 0x40000000,
    2: 0x40400000,
    3: 0x40404000,
}
EXPECTED_SHA256 = {
    0: "b8426b5bad113fd8710fc261385851e6005917eab7401093b6e7087042f82a39",
    1: "c596e9d32329e0bdd74880dbc8bf401b2f592ec73d46c0f94ec9efb8700f61f6",
    2: "891cda1ff9625609bff76b734f3f5923097aed13147d6f7142447934f7bf9689",
    3: "739175c661e467abf9f9f7fb0dde8f9f6b25a183c2f264b2bdbbca888b76ef9e",
}


def build_bits(error_count: int) -> list[int]:
    if error_count not in ERROR_MASKS:
        raise ValueError("error_count must be one of 0, 1, 2, 3")

    address_info = ((base.CAPCODE >> 3) << 2) | base.FUNCTION_BITS
    address_word = base.encode_codeword(address_info)
    message_words = base.encode_alpha(MESSAGE)
    assert len(message_words) == 5

    # Corrupt only the first message codeword.  The selected bits are spaced
    # apart so the fixture is deterministic and does not accidentally alter
    # the address or sync word.
    message_words[0] ^= ERROR_MASKS[error_count]

    batch = [address_word, *message_words]
    batch.extend([base.IDLE] * (16 - len(batch)))

    bits = [1 if index % 2 == 0 else 0 for index in range(base.PREAMBLE_BITS)]
    bits.extend(base.word_bits(base.SYNC))
    bits.extend(bit for word in batch for bit in base.word_bits(word))
    return bits


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {Path(sys.argv[0]).name} OUTPUT.wav ERROR_COUNT", file=sys.stderr)
        return 2

    output = Path(sys.argv[1])
    try:
        error_count = int(sys.argv[2])
        bits = build_bits(error_count)
    except (ValueError, KeyError) as exc:
        print(str(exc), file=sys.stderr)
        return 2

    wav = base.make_wav(base.build_pcm(bits))
    digest = hashlib.sha256(wav).hexdigest()
    expected = EXPECTED_SHA256[error_count]
    if digest != expected:
        print(f"fixture SHA-256 mismatch: expected={expected} actual={digest}", file=sys.stderr)
        return 3

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(wav)
    print(
        f"generated {output} message_errors={error_count} "
        f"({len(wav)} bytes, sha256={digest})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
