#!/usr/bin/env python3
"""Generate a privacy-safe synthetic MOBITEX-8000 preservation WAV.

The waveform exercises PDW's real legacy MOBITEX zero-crossing slicer,
ROSI sync, header FEC, bit scrambler, 240-bit interleave/FEC block path and
MPAK formatting. No received/off-air traffic is used.
"""

from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

SAMPLE_RATE = 44100
BITRATE = 8000
BITSYNC = 0xCCCC
FRSYNC = 0xEB90
DESTINATION = 1234567
SENDER = 7654321
EXPECTED_SHA256 = "26db967395e8792e21e34526e9392bf2616de3c19fe16ef888b4904e9e481176"

FEC_MATRICES = (0xEC, 0xD3, 0xBA, 0x75)


def bits_msb(value: int, width: int) -> list[int]:
    return [(value >> (width - 1 - index)) & 1 for index in range(width)]


def fec_nibble(data: int) -> int:
    result = 0
    for shift, matrix in zip((3, 2, 1, 0), FEC_MATRICES):
        result |= ((data & matrix).bit_count() & 1) << shift
    return result


def codeword12(data: int) -> int:
    assert 0 <= data <= 0xFF
    return (data << 4) | fec_nibble(data)


def build_frame_header() -> list[int]:
    # BaseID=1, AreaID=2, CFlags=0. The two FEC nibbles are packed into the
    # third byte exactly as MOBITEX::frame_sync() extracts them.
    base_id = 1
    area_id = 2
    cflags = 0

    first = ((base_id & 0x3F) << 2) | ((area_id >> 4) & 0x03)
    second = ((area_id & 0x0F) << 4) | (cflags & 0x0F)
    parity = (fec_nibble(first) << 4) | fec_nibble(second)

    assert [first, second, parity] == [0x04, 0x20, 0x9B]
    assert codeword12(first) == 0x049
    assert codeword12(second) == 0x20B
    return [first, second, parity]


def int24(value: int) -> list[int]:
    assert 0 <= value <= 0xFFFFFF
    return [(value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF]


def build_data_bytes() -> list[int]:
    data = [0] * 20

    # Link-control header: MRM, one block, 18 meaningful data bytes.
    data[0:3] = int24(DESTINATION)
    data[3] = 0x21  # FrameID=MRM (1), BytesLastBlock high bit=16.
    data[4] = 0x20  # BytesLastBlock low nibble=2, sequence=0 => 18 total.
    data[5] = 0x01  # BlockLength=1.

    # MPAK address pair. Legacy GetMpakHeader() chooses address2 as sender
    # when address1 equals the link-control destination.
    data[6:9] = int24(DESTINATION)
    data[9:12] = int24(SENDER)
    data[12] = 0x00
    data[13] = 0xC7  # DTE service class (3), ACTIVE type (7).

    # Synthetic ESN rendered by the legacy MPAK formatter as 42.17.4660.
    data[14] = 42
    data[15] = 0x44  # model=17, top two ID bits=0.
    data[16] = 0x12
    data[17] = 0x34

    # The legacy block parser accepts a block with clean FEC even when CRC is
    # not the deciding acceptance condition. Keep both trailer bytes explicit
    # and FEC-valid rather than relying on random padding.
    data[18] = 0x00
    data[19] = 0x00

    assert data[0:3] == [0x12, 0xD6, 0x87]
    assert data[9:12] == [0x74, 0xCB, 0xB1]
    return data


def interleave_block(data: list[int]) -> list[int]:
    assert len(data) == 20
    codewords = [codeword12(value) for value in data]

    # MOBITEX::barfrog() reconstructs codeword i from obm[j*20+i], j=0..11,
    # shifting left each time. Emit those 12 rows in exactly that order.
    bits = [
        (codewords[index] >> (11 - row)) & 1
        for row in range(12)
        for index in range(20)
    ]
    assert len(bits) == 240

    # Verify the inverse before scrambling.
    reconstructed = []
    for index in range(20):
        value = 0
        for row in range(12):
            value = (value << 1) | bits[row * 20 + index]
        reconstructed.append(value >> 4)
    assert reconstructed == data
    return bits


def scrambler_bits(count: int) -> list[int]:
    # Exact inverse companion to MOBITEX::mb_bs(): reset state is 0x1E and
    # taps at positions 1 and 5 are XORed before the shift/update.
    state = 0x1E
    output: list[int] = []

    for _ in range(count):
        bit = int(bool(state & 0x01) ^ bool(state & 0x10))
        state >>= 1
        if bit:
            state ^= 0x100
        output.append(bit)

    return output


def build_payload_bits() -> list[int]:
    header = build_frame_header()
    clear_block = interleave_block(build_data_bytes())
    scrambling = scrambler_bits(len(clear_block))
    wire_block = [bit ^ scramble for bit, scramble in zip(clear_block, scrambling)]

    return (
        bits_msb(BITSYNC, 16)
        + bits_msb(FRSYNC, 16)
        + [bit for byte in header for bit in bits_msb(byte, 8)]
        + wire_block
    )


def build_bits() -> list[int]:
    # Give Reset_ATB()/MOBITEX_To_Bits() a deterministic transition history
    # before the 32-bit ROSI sync appears.
    prefix = [1, 0] * 16
    payload = build_payload_bits()
    bits = prefix + payload
    assert len(bits) == 328
    return bits


def build_pcm(bits: list[int]) -> bytes:
    sample_count = (len(bits) * SAMPLE_RATE + BITRATE - 1) // BITRATE
    samples = bytearray(sample_count)

    for sample_index in range(sample_count):
        bit_index = min((sample_index * BITRATE) // SAMPLE_RATE, len(bits) - 1)
        # PDW XORs unsigned 8-bit PCM with 0x80 before comparing signed levels.
        samples[sample_index] = 0xFF if bits[bit_index] else 0x00

    return bytes(samples)


def signed_after_legacy_xor(sample: int) -> int:
    value = sample ^ 0x80
    return value - 256 if value >= 128 else value


def emulate_legacy_slicer(pcm: bytes) -> list[int]:
    # Small executable specification of MOBITEX_To_Bits(). It validates that
    # sample quantization and zero-crossing resync recover the intended payload
    # before the WAV is allowed into the preservation corpus.
    watch_step = SAMPLE_RATE / BITRATE
    clkt_lo = 0.18
    clkt_hi = 2.02
    atb_bit = 0
    atb_len = 0
    watch_ctr = -1.0
    decoded: list[int] = []

    for sample_index, sample in enumerate(pcm):
        value = signed_after_legacy_xor(sample)
        atb_len += 1

        if value < -1 and atb_bit == 1:
            atb_bit = 0
            ratio = atb_len / watch_step
            if atb_len < watch_step * 2 and clkt_lo < ratio < clkt_hi:
                watch_ctr = sample_index + watch_step / 2
        elif value > -1 and atb_bit == 0:
            atb_bit = 1

        # Legacy MOBITEX_To_Bits() resets this counter every sample.
        atb_len = 0

        if watch_ctr != -1 and watch_ctr - sample_index < 1:
            decoded.append(atb_bit)
            watch_ctr += watch_step

    return decoded


def validate_pcm(pcm: bytes) -> None:
    payload = build_payload_bits()
    decoded = emulate_legacy_slicer(pcm)
    matches = [
        index
        for index in range(len(decoded) - len(payload) + 1)
        if decoded[index:index + len(payload)] == payload
    ]

    # The leading transition history loses one sampled prefix bit; the complete
    # 296-bit sync/header/scrambled block must still appear once and untouched.
    assert matches == [31]


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

    pcm = build_pcm(build_bits())
    validate_pcm(pcm)
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
