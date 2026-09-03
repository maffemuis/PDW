#!/usr/bin/env python3
"""Generate a privacy-safe synthetic FLEX-1600 tone-only preservation WAV.

The waveform is built from fixed protocol fields only. It contains no
received/off-air pager traffic. The layout is deliberately minimal: FLEX-1600
2-level sync, one valid cycle-information codeword, one BIW, one short address,
one SH/TONE vector and deterministic filler until PDW can terminate the frame.
"""

from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

SAMPLE_RATE = 44100
BAUD_RATE = 1600
CRC_GENERATOR = 0x769
CAPCODE = 123456

SYNC_WORDS = (0x870C, 0xA6C6, 0xAAAA, 0x78F3)
EXPECTED_SHA256 = "3571da8b384438a48fd8f4ff5e35e5d2c60f588542e8bf76d2d9550312582812"

# Values as PDW stores them in FLEX::frame[] after deinterleaving/ECC.
# BIW: address field starts at word 1, vector field at word 2.
# Vector: mode 2 (SH/TONE), tone subtype 1.
FRAME_WORDS = [
    0x000807,
    CAPCODE + 32768,
    0x0000A5,
    0x156666,
    0x151111,
    0x150000,
    0x153333,
    0x152222,
    0x15DDDD,
    0x15CCCC,
    0x15FFFF,
    0x15EEEE,
    0x159999,
    0x158888,
    0x15BBBB,
    0x000000,
]


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


def reverse21(value: int) -> int:
    result = 0
    for bit in range(21):
        result = (result << 1) | ((value >> bit) & 1)
    return result


def flex_wire_info(frame_value: int) -> int:
    # FLEX::showblock reconstructs each 21-bit frame value by reversing and
    # complementing the received data bits. Apply the inverse here before BCH.
    return (~reverse21(frame_value)) & 0x1FFFFF


def xsum(value: int) -> int:
    total = value & 0xF
    total += (value >> 4) & 0xF
    total += (value >> 8) & 0xF
    total += (value >> 12) & 0xF
    total += (value >> 16) & 0xF
    total += (value >> 20) & 0x1
    return total & 0xF


def bits16(value: int) -> list[int]:
    return [(value >> (15 - bit)) & 1 for bit in range(16)]


def bits32(value: int) -> list[int]:
    return [(value >> (31 - bit)) & 1 for bit in range(32)]


def interleave_block(values: list[int]) -> list[int]:
    assert len(values) == 8
    codewords = [encode_codeword(flex_wire_info(value)) for value in values]
    encoded = [bits32(word) for word in codewords]
    return [encoded[word][bit] for bit in range(32) for word in range(8)]


def build_bits() -> list[int]:
    assert len(FRAME_WORDS) == 16
    assert FRAME_WORDS[1] == 0x026240
    assert xsum(FRAME_WORDS[0]) == 0xF
    assert ((FRAME_WORDS[0] >> 8) & 0x3) + 1 == 1
    assert ((FRAME_WORDS[0] >> 10) & 0x3F) == 2
    assert xsum(FRAME_WORDS[2]) == 0xF
    assert ((FRAME_WORDS[2] >> 4) & 0x7) == 2
    assert ((FRAME_WORDS[2] >> 7) & 0x3) == 1

    first_three = [
        encode_codeword(flex_wire_info(value)) for value in FRAME_WORDS[:3]
    ]
    assert first_three == [0x1FEFFD67, 0xFDB9BCA2, 0x5AFFFE67]

    # Give the legacy 1600-baud slicer a clean clock before the 64-bit sync.
    preamble = [1 if bit % 2 == 0 else 0 for bit in range(128)]
    sync = [bit for word in SYNC_WORDS for bit in bits16(word)]

    # frame_flex() enters an 89-bit sync holdoff. The first post-sync call
    # leaves 88 bits: 16 ignored, 32-bit cycle info, then 40 ignored.
    holdoff_before_cycle = [1 if bit % 2 == 0 else 0 for bit in range(16)]
    cycle_info = bits32(encode_codeword(0x1FFFFF))
    assert cycle_info == [1] * 32
    holdoff_after_cycle = [1 if bit % 2 == 0 else 0 for bit in range(40)]

    data = interleave_block(FRAME_WORDS[:8])
    data += interleave_block(FRAME_WORDS[8:])
    assert len(data) == 512

    return preamble + sync + holdoff_before_cycle + cycle_info + holdoff_after_cycle + data


def build_pcm(bits: list[int]) -> bytes:
    sample_count = (len(bits) * SAMPLE_RATE + BAUD_RATE - 1) // BAUD_RATE
    samples = bytearray(sample_count)

    for sample_index in range(sample_count):
        bit_index = min((sample_index * BAUD_RATE) // SAMPLE_RATE, len(bits) - 1)
        # PDW's Audio_To_Bits() XORs unsigned 8-bit PCM with 0x80.
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

    bits = build_bits()
    assert len(bits) == 792
    wav = make_wav(build_pcm(bits))
    digest = hashlib.sha256(wav).hexdigest()

    if digest != EXPECTED_SHA256:
        print(f"fixture SHA-256 mismatch: {digest}", file=sys.stderr)
        return 3

    output.write_bytes(wav)
    print(f"generated {output} ({len(wav)} bytes, sha256={digest})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
