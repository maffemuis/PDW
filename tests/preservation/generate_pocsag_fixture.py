#!/usr/bin/env python3
"""Generate the first privacy-safe PDW POCSAG preservation fixture.

The fixture is entirely synthetic. It represents one POCSAG-1200 alpha page:

    capcode: 123456
    function: 4
    message: PDW PRESERVATION OK!

No received/off-air traffic is used.
"""

from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

SYNC = 0x7CD215D8
IDLE = 0x7A89C197
CRC_GENERATOR = 0x769
PREAMBLE_BITS = 576
SAMPLE_RATE = 44100
BAUD_RATE = 1200
CAPCODE = 123456
FUNCTION_BITS = 0x3
MESSAGE = "PDW PRESERVATION OK!"
EXPECTED_SHA256 = "9800babd1d2f789a95d1622be3ad47aa9e9fc8597c5a254b9a29bb54514ada70"


def crc21(info: int) -> int:
    denominator = CRC_GENERATOR << 20
    value = info << 10

    for column in range(21):
        if (value >> (30 - column)) & 1:
            value ^= denominator
        denominator >>= 1

    return value & 0x3FF


def parity(value: int) -> int:
    result = 0
    for _ in range(32):
        result ^= value & 1
        value >>= 1
    return result


def encode_codeword(info: int) -> int:
    with_crc = (info << 10) | crc21(info)
    return (with_crc << 1) | parity(with_crc)


def encode_alpha(text: str) -> list[int]:
    payload_bits: list[int] = []

    for char in text.encode("ascii"):
        # POCSAG alpha characters are 7-bit ASCII, least-significant bit first.
        payload_bits.extend((char >> bit) & 1 for bit in range(7))

    codewords: list[int] = []
    for offset in range(0, len(payload_bits), 20):
        chunk = payload_bits[offset : offset + 20]
        chunk.extend([0] * (20 - len(chunk)))

        payload = 0
        for bit in chunk:
            payload = (payload << 1) | bit

        codewords.append(encode_codeword(0x100000 | payload))

    return codewords


def word_bits(word: int) -> list[int]:
    return [(word >> (31 - bit)) & 1 for bit in range(32)]


def build_transmission_bits() -> list[int]:
    assert CAPCODE & 0x7 == 0, "fixture expects the address in frame 0"
    assert len(MESSAGE) == 20, "20 chars deliberately force legacy ALPHA classification"

    address_info = ((CAPCODE >> 3) << 2) | FUNCTION_BITS
    address_word = encode_codeword(address_info)
    message_words = encode_alpha(MESSAGE)

    # Freeze the protocol encoder itself as part of the preservation fixture.
    assert address_word == 0x0789182E
    assert message_words == [
        0x8523D0CB,
        0xC1054F66,
        0xB472D18E,
        0x94AD66B2,
        0x89593C5E,
        0xE5C82AD0,
        0xF3A6158A,
    ]

    # One complete batch: address + seven message words + terminating IDLE,
    # then IDLE padding. The sync word precedes the 16-word batch.
    batch = [address_word, *message_words, IDLE, *([IDLE] * 7)]
    assert len(batch) == 16

    bits = [1 if index % 2 == 0 else 0 for index in range(PREAMBLE_BITS)]
    bits.extend(word_bits(SYNC))
    for word in batch:
        bits.extend(word_bits(word))

    return bits


def build_pcm(bits: list[int]) -> bytes:
    sample_count = (len(bits) * SAMPLE_RATE + BAUD_RATE - 1) // BAUD_RATE
    samples = bytearray(sample_count)

    for sample_index in range(sample_count):
        bit_index = min((sample_index * BAUD_RATE) // SAMPLE_RATE, len(bits) - 1)
        # PDW's legacy Audio_To_Bits() XORs unsigned 8-bit PCM with 0x80.
        # Use full-scale values for an intentionally unambiguous synthetic signal.
        samples[sample_index] = 0xFF if bits[bit_index] else 0x00

    return bytes(samples)


def make_wav(pcm: bytes) -> bytes:
    header = (
        b"RIFF"
        + struct.pack("<I", 36 + len(pcm))
        + b"WAVE"
        + b"fmt "
        + struct.pack("<IHHIIHH", 16, 1, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8)
        + b"data"
        + struct.pack("<I", len(pcm))
    )
    return header + pcm


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUTPUT.wav", file=sys.stderr)
        return 2

    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)

    wav = make_wav(build_pcm(build_transmission_bits()))
    digest = hashlib.sha256(wav).hexdigest()
    if digest != EXPECTED_SHA256:
        print(f"fixture SHA-256 mismatch: {digest}", file=sys.stderr)
        return 3

    output.write_bytes(wav)
    print(f"generated {output} ({len(wav)} bytes, sha256={digest})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
