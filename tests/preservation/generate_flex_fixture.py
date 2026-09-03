#!/usr/bin/env python3
"""Generate privacy-safe synthetic FLEX-1600 preservation WAVs.

Every waveform is built from fixed protocol fields only. No received/off-air
pager traffic is used. The fixtures exercise the real legacy 44.1 kHz audio
slicer, FLEX sync, BCH/ECC, interleaving and message formatting paths.
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
EXPECTED_SHA256 = {
    "tone": "b1af5b7ee1d04dd6636d8c1ddac7a86ec35f70fec0360361c9f0679e5f879c9a",
    "alpha": "315a0a3a12efc99df1fb9ee199f0625732d079855b362e1c851fb174e74a98cc",
}
ALPHA_MESSAGE = "FLEX GOLDEN OK!"

TONE_FRAME_WORDS = [
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


def pack_alpha_word(text: str) -> int:
    assert len(text) == 3
    return ord(text[0]) | (ord(text[1]) << 7) | (ord(text[2]) << 14)


def alpha_data_words(message: str) -> list[int]:
    # Legacy FLEX::showframe() treats fragment number 3 as an unfragmented
    # message, but still skips the low 7 bits of the first post-header word.
    # Keep that historical header slot explicit and terminate unused visible
    # character positions with ETX so the requested payload is exact.
    characters = "\x00" + message + "\x03\x03"
    assert len(characters) == 18
    return [
        pack_alpha_word(characters[index:index + 3])
        for index in range(0, len(characters), 3)
    ]


ALPHA_FRAME_WORDS = [
    0x000807,                 # BIW: asa=1, vsa=2
    CAPCODE + 32768,          # short address
    0x01C1D4,                 # ALPHA vector: header at 3, seven words total
    0x001800,                 # fragment header, fragment number 3
    *alpha_data_words(ALPHA_MESSAGE),
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


def frame_words(kind: str) -> list[int]:
    if kind == "tone":
        return list(TONE_FRAME_WORDS)
    if kind == "alpha":
        return list(ALPHA_FRAME_WORDS)
    raise ValueError(f"unsupported FLEX fixture kind: {kind}")


def validate_frame_words(kind: str, words: list[int]) -> None:
    assert len(words) == 16
    assert words[1] == 0x026240
    assert xsum(words[0]) == 0xF
    assert ((words[0] >> 8) & 0x3) + 1 == 1
    assert ((words[0] >> 10) & 0x3F) == 2
    assert xsum(words[2]) == 0xF

    if kind == "tone":
        assert ((words[2] >> 4) & 0x7) == 2
        assert ((words[2] >> 7) & 0x3) == 1
        first = [encode_codeword(flex_wire_info(value)) for value in words[:3]]
        assert first == [0x1FEFFD67, 0xFDB9BCA2, 0x5AFFFE67]
        return

    assert kind == "alpha"
    assert len(ALPHA_MESSAGE) == 15
    assert ((words[2] >> 4) & 0x7) == 5
    assert ((words[2] >> 7) & 0x7F) == 3
    assert ((words[2] >> 14) & 0x7F) == 7
    assert ((words[3] >> 11) & 0x3) == 3
    assert words[4:10] == [
        0x132300,
        0x082C45,
        0x1327C7,
        0x13A2C4,
        0x12E7A0,
        0x00C1A1,
    ]
    first = [encode_codeword(flex_wire_info(value)) for value in words[:4]]
    assert first == [0x1FEFFD67, 0xFDB9BCA2, 0xD47C79A3, 0xFFE7FCC6]


def build_bits(kind: str) -> list[int]:
    words = frame_words(kind)
    validate_frame_words(kind, words)

    # Give the legacy 1600-baud slicer a clean clock before the 64-bit sync.
    preamble = [1 if bit % 2 == 0 else 0 for bit in range(128)]
    sync = [bit for word in SYNC_WORDS for bit in bits16(word)]

    # frame_flex() enters an 89-bit sync holdoff. The first post-sync call
    # leaves 88 bits: 16 ignored, 32-bit cycle info, then 40 ignored.
    holdoff_before_cycle = [1 if bit % 2 == 0 else 0 for bit in range(16)]
    cycle_info = bits32(encode_codeword(0x1FFFFF))
    assert cycle_info == [1] * 32
    holdoff_after_cycle = [1 if bit % 2 == 0 else 0 for bit in range(40)]

    data = interleave_block(words[:8])
    data += interleave_block(words[8:])
    assert len(data) == 512

    # The 44.1 kHz legacy slicer consumes one transition symbol at the
    # recognized sync-to-data boundary. Keep that real audio-path behavior
    # explicit instead of bypassing or modifying the decoder.
    alignment_pad = [1]

    return (
        preamble
        + sync
        + holdoff_before_cycle
        + cycle_info
        + holdoff_after_cycle
        + alignment_pad
        + data
    )


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
    if len(sys.argv) not in (2, 3):
        print(f"usage: {Path(sys.argv[0]).name} OUTPUT.wav [tone|alpha]", file=sys.stderr)
        return 2

    output = Path(sys.argv[1])
    kind = sys.argv[2].lower() if len(sys.argv) == 3 else "tone"

    if kind not in EXPECTED_SHA256:
        print(f"unsupported FLEX fixture kind: {kind}", file=sys.stderr)
        return 2

    output.parent.mkdir(parents=True, exist_ok=True)

    bits = build_bits(kind)
    assert len(bits) == 793
    wav = make_wav(build_pcm(bits))
    digest = hashlib.sha256(wav).hexdigest()

    if digest != EXPECTED_SHA256[kind]:
        print(f"fixture SHA-256 mismatch: {digest}", file=sys.stderr)
        return 3

    output.write_bytes(wav)
    print(f"generated {output} ({len(wav)} bytes, kind={kind}, sha256={digest})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
