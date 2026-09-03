#!/usr/bin/env python3
"""Generate privacy-safe synthetic P2000/POCSAG regression fixtures.

The fixtures are not captured traffic. They deliberately exercise decoder
edge cases with deterministic, synthetic codewords.
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
NUMERIC_ALPHABET = "0123456789*U -]["

CASES = {
    "short-alpha": {
        "kind": "alpha",
        "message": "BRANDWEER TEST",
        "address_error_mask": 0x0,
        "sha256": "b8426b5bad113fd8710fc261385851e6005917eab7401093b6e7087042f82a39",
    },
    "short-alpha-divisor": {
        "kind": "alpha",
        # Five message words again, but unlike the original short-alpha fixture
        # the legacy %6 remainder window starts with a one while the proper %7
        # trailing remainder is 00. This makes the old classifier observably
        # choose NUMERIC instead of ALPHA.
        "message": "BRANDWEER 1234",
        "address_error_mask": 0x0,
        "sha256": "841a9c76d5c7defb1ea3da6334fcf950727d78f10e42476ab6e4277bebf6082c",
    },
    "numeric-binary-remainder": {
        "kind": "numeric",
        # One numeric message word whose alpha remainder window is 0,1,1,0,0,1.
        # The legacy strncpy/strchr code treats the first binary zero as a C
        # string terminator and therefore misses the later one-bits, allowing
        # the otherwise plausible alpha interpretation "MP" to win.
        "message": "-48*9",
        "address_error_mask": 0x0,
        "sha256": "f1612baa84925805fecdbbcedbf69e04663b8cbba9f985e3f1a74ccb163100d3",
    },
    "bad-address": {
        "kind": "alpha",
        "message": "BRANDWEER TEST",
        # Flip three BCH check bits only. Syndrome 0x007 has no entry in
        # PDW's one/two-bit correction table, so ecd() deterministically
        # reports this address word as uncorrectable while leaving its
        # address payload bits untouched.
        "address_error_mask": 0xE,
        "sha256": "dad365218e4acdd614a5a368a4a5acbc83fd3b61b641b7f2b1cfc5955df6b71d",
    },
}


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


def payload_codewords(payload_bits: list[int]) -> list[int]:
    codewords: list[int] = []
    for offset in range(0, len(payload_bits), 20):
        chunk = payload_bits[offset : offset + 20]
        chunk.extend([0] * (20 - len(chunk)))
        payload = 0
        for bit in chunk:
            payload = (payload << 1) | bit
        codewords.append(encode_codeword(0x100000 | payload))
    return codewords


def encode_alpha(text: str) -> list[int]:
    payload_bits: list[int] = []
    for char in text.encode("ascii"):
        payload_bits.extend((char >> bit) & 1 for bit in range(7))
    return payload_codewords(payload_bits)


def encode_numeric(text: str) -> list[int]:
    payload_bits: list[int] = []
    for char in text:
        value = NUMERIC_ALPHABET.index(char)
        payload_bits.extend((value >> bit) & 1 for bit in range(4))
    return payload_codewords(payload_bits)


def word_bits(word: int) -> list[int]:
    return [(word >> (31 - bit)) & 1 for bit in range(32)]


def build_bits(case_name: str) -> list[int]:
    case = CASES[case_name]
    assert CAPCODE & 0x7 == 0
    address_info = ((CAPCODE >> 3) << 2) | FUNCTION_BITS
    address_word = encode_codeword(address_info) ^ int(case["address_error_mask"])

    if case["kind"] == "alpha":
        message_words = encode_alpha(str(case["message"]))
    elif case["kind"] == "numeric":
        message_words = encode_numeric(str(case["message"]))
    else:
        raise ValueError(f"unsupported fixture kind: {case['kind']}")

    if case_name in {"short-alpha", "short-alpha-divisor", "bad-address"}:
        assert len(message_words) == 5, "fixture must stay on the five-codeword edge case"
    elif case_name == "numeric-binary-remainder":
        assert message_words == [0xD90ACBA6]

    batch = [address_word, *message_words]
    batch.extend([IDLE] * (16 - len(batch)))

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
        print(
            f"usage: {Path(sys.argv[0]).name} OUTPUT.wav "
            "[short-alpha|short-alpha-divisor|numeric-binary-remainder|bad-address]",
            file=sys.stderr,
        )
        return 2

    output = Path(sys.argv[1])
    case_name = sys.argv[2] if len(sys.argv) == 3 else "short-alpha"
    if case_name not in CASES:
        print(f"unknown fixture case: {case_name}", file=sys.stderr)
        return 2

    output.parent.mkdir(parents=True, exist_ok=True)
    wav = make_wav(build_pcm(build_bits(case_name)))
    digest = hashlib.sha256(wav).hexdigest()
    expected_digest = str(CASES[case_name]["sha256"])
    if digest != expected_digest:
        print(f"fixture SHA-256 mismatch: {digest}", file=sys.stderr)
        return 3
    output.write_bytes(wav)
    print(
        f"generated {output} case={case_name} "
        f"({len(wav)} bytes, sha256={digest})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
