#!/usr/bin/env python3
"""Generate a privacy-safe synthetic ACARS preservation WAV.

The fixture contains only fixed synthetic protocol fields. It models the
legacy PDW ACARS zero-crossing demodulator directly: at 2400 baud a repeated
bit uses two half-wave crossings, while a changed bit uses one full-bit
crossing. No received/off-air traffic is used.
"""

from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

SAMPLE_RATE = 44100
BAUD_RATE = 2400
SAMPLES_PER_BIT = SAMPLE_RATE / BAUD_RATE
EXPECTED_SHA256 = "bf81f60be31967a03d34483ac62d38f810fee84f7502f88dfccf38531c1a937f"

SYNC_WORDS = (0xFFFF, 0x2AAB, 0x1616)


def odd_parity_byte(value: int) -> int:
    value &= 0x7F
    if value.bit_count() % 2 == 0:
        value |= 0x80
    return value


def bits_lsb(value: int, width: int) -> list[int]:
    return [(value >> bit) & 1 for bit in range(width)]


def encoded_char(value: int | str) -> int:
    if isinstance(value, str):
        assert len(value) == 1
        value = ord(value)
    return odd_parity_byte(value)


def build_bits() -> list[int]:
    # frame() shifts incoming bits into the MSB, so ACARS bytes/words are sent
    # least-significant bit first to reconstruct these legacy sync registers.
    bits: list[int] = []
    for word in SYNC_WORDS:
        bits.extend(bits_lsb(word, 16))

    # SOH is compared as the exact byte 0x01; it already has odd parity.
    bits.extend(bits_lsb(0x01, 8))

    payload = [
        encoded_char("2"),                       # mode
        *[encoded_char(c) for c in ".N12345"],  # 7-byte aircraft address
        encoded_char(0x06),                      # ACK
        encoded_char("Q"), encoded_char("0"),   # label
        encoded_char("A"),                       # DBI
        encoded_char(0x00),                      # no STX / no text payload
        *[encoded_char(c) for c in "M001"],      # message number
        *[encoded_char(c) for c in "AB1234"],    # flight number
        encoded_char(0x03),                      # ETX consumed by index 12
        encoded_char(0x00),                      # byte that advances index 13/output
    ]

    assert len(payload) == 25
    assert all(byte.bit_count() % 2 == 1 for byte in payload)

    for byte in payload:
        bits.extend(bits_lsb(byte, 8))

    assert len(bits) == 256
    return bits


def build_pcm(bits: list[int]) -> bytes:
    # ACARS_To_Bits starts with atb_bit=0. Schedule an accepted zero crossing
    # at every bit boundary. If the desired bit is unchanged, add a half-bit
    # crossing first; the legacy `< 12 samples` guard discards that crossing
    # and accepts the second, returning to the same bit value.
    events: dict[int, int] = {}
    state = 0

    def toggle_at(sample_index: int) -> None:
        events[sample_index] = events.get(sample_index, 0) ^ 1

    for index, bit in enumerate(bits):
        if bit == state:
            toggle_at(round((index + 0.5) * SAMPLES_PER_BIT))
            state ^= 1
        toggle_at(round((index + 1.0) * SAMPLES_PER_BIT))
        state ^= 1
        assert state == bit

    sample_count = round(len(bits) * SAMPLES_PER_BIT) + 2
    pcm = bytearray(sample_count)
    state = 0

    for sample_index in range(sample_count):
        if events.get(sample_index, 0):
            state ^= 1
        pcm[sample_index] = 0xFF if state else 0x00

    return bytes(pcm)


def emulate_legacy_demod(pcm: bytes) -> list[int]:
    atb_bit = 0
    atb_len = 0
    decoded: list[int] = []

    for sample in pcm:
        value = sample ^ 0x80
        if value >= 0x80:
            value -= 0x100

        atb_len += 1
        process = False

        if value < 0 and atb_bit == 1:
            atb_bit = 0
            if atb_len < 12:
                continue
            process = True
            atb_len = 0
        elif value > 0 and atb_bit == 0:
            atb_bit = 1
            if atb_len < 12:
                continue
            process = True
            atb_len = 0

        if process:
            decoded.append(atb_bit)

    return decoded


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
    pcm = build_pcm(bits)
    assert emulate_legacy_demod(pcm) == bits

    wav = make_wav(pcm)
    digest = hashlib.sha256(wav).hexdigest()
    if digest != EXPECTED_SHA256:
        print(f"fixture SHA-256 mismatch: {digest}", file=sys.stderr)
        return 3

    output.write_bytes(wav)
    print(f"generated {output} ({len(wav)} bytes, sha256={digest})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
