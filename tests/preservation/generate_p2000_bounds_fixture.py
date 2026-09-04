#!/usr/bin/env python3
"""Generate a deterministic synthetic POCSAG stream that crosses the alpha buffer boundary.

This is deliberately synthetic/privacy-safe.  It keeps one POCSAG message open
across many batches so the legacy decoder must not write beyond MAX_STR_LEN.
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
MESSAGE_LENGTH = 5128
EXPECTED_SHA256 = "2269408dd6b53148fb1fa2a3d55477fcf8852a9a1b7a840fa7bbf8e5ccd34d88"


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


def word_bits(word: int) -> list[int]:
    return [(word >> (31 - bit)) & 1 for bit in range(32)]


def encode_alpha(text: str) -> list[int]:
    payload_bits: list[int] = []
    for char in text.encode("ascii"):
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


def build_bits() -> list[int]:
    assert CAPCODE & 0x7 == 0
    address_info = ((CAPCODE >> 3) << 2) | FUNCTION_BITS
    address_word = encode_codeword(address_info)
    message_words = encode_alpha("A" * MESSAGE_LENGTH)

    bits = [1 if index % 2 == 0 else 0 for index in range(PREAMBLE_BITS)]
    word_index = 0
    first_batch = True

    while first_batch or word_index < len(message_words):
        bits.extend(word_bits(SYNC))
        batch: list[int] = []
        if first_batch:
            batch.append(address_word)
            first_batch = False

        capacity = 16 - len(batch)
        take = min(capacity, len(message_words) - word_index)
        batch.extend(message_words[word_index : word_index + take])
        word_index += take

        if word_index >= len(message_words) and len(batch) < 16:
            batch.append(IDLE)
        batch.extend([IDLE] * (16 - len(batch)))

        for word in batch:
            bits.extend(word_bits(word))

        if word_index >= len(message_words):
            break

    return bits


def build_pcm(bits: list[int]) -> bytes:
    sample_count = (len(bits) * SAMPLE_RATE + BAUD_RATE - 1) // BAUD_RATE
    samples = bytearray(sample_count)
    for sample_index in range(sample_count):
        bit_index = min((sample_index * BAUD_RATE) // SAMPLE_RATE, len(bits) - 1)
        samples[sample_index] = 0xFF if bits[bit_index] else 0x00
    return bytes(samples)


def make_wav(pcm: bytes) -> bytes:
    return (
        b"RIFF"
        + struct.pack("<I", 36 + len(pcm))
        + b"WAVE"
        + b"fmt "
        + struct.pack("<IHHIIHH", 16, 1, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8)
        + b"data"
        + struct.pack("<I", len(pcm))
        + pcm
    )


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUTPUT.wav", file=sys.stderr)
        return 2

    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    wav = make_wav(build_pcm(build_bits()))
    digest = hashlib.sha256(wav).hexdigest()
    if digest != EXPECTED_SHA256:
        print(f"fixture SHA-256 mismatch: {digest}", file=sys.stderr)
        return 3

    output.write_bytes(wav)
    print(
        f"generated {output} ({MESSAGE_LENGTH} alpha chars, {len(wav)} bytes, "
        f"sha256={digest})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
