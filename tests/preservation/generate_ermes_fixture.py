#!/usr/bin/env python3
"""Generate privacy-safe synthetic ERMES serial symbols for preservation.

ERMES is not connected to PDW's current live sound-card path. The legacy
serial decoder feeds four-level modem symbols directly to ERMES::frame().
This generator therefore emits one byte per real legacy symbol (0..3) rather
than inventing a WAV route that the application does not have.
"""

from __future__ import annotations

import hashlib
import sys
from pathlib import Path

SYNC32 = 0x2288282A
APT = 0x09C461C9
MDEL = 0x3777E7AB
CAPCODE = 123456
EXPECTED_SHA256 = "053a1bdc37e1151c9b4ae7a7c9635dec93e1c8252ff13313489f0192c458a63e"
G_MATRIX = (
    0xF08, 0x784, 0x3C2, 0x1E1, 0xD96, 0x6CB, 0xE03, 0xA67, 0x855,
    0x94C, 0x4A6, 0x253, 0xC4F, 0xB41, 0x8C6, 0x463, 0xF57, 0xACD,
)

# ERMES::frame() receives the number of high modem status lines. Each symbol
# maps to two Gray-coded raw bits: 0=>00, 1=>01, 2=>11, 3=>10.
PAIR_TO_SYMBOL = {0b00: 0, 0b01: 1, 0b10: 3, 0b11: 2}


def reverse_bits(value: int, width: int) -> int:
    result = 0
    for bit in range(width):
        result = (result << 1) | ((value >> bit) & 1)
    return result


def encode_data_codeword(decoded_value: int) -> int:
    """Invert ERMES::checkecc() for one error-free 18-bit decoded value."""
    assert 0 <= decoded_value < (1 << 18)

    raw_data = reverse_bits(decoded_value, 18)
    ecc = 0
    for bit, matrix in enumerate(G_MATRIX):
        if (raw_data >> bit) & 1:
            ecc ^= matrix

    raw_ecc = reverse_bits(ecc, 12)
    codeword = raw_data | (raw_ecc << 18)
    assert decode_data_codeword(codeword) == (decoded_value, 0)
    return codeword


def decode_data_codeword(codeword: int) -> tuple[int, int]:
    """Small executable specification of the no-error ERMES::checkecc path."""
    value = codeword
    ecc = 0
    decoded = 0

    for bit, matrix in enumerate(G_MATRIX):
        decoded <<= 1
        if value & 1:
            ecc ^= matrix
            decoded |= 1
        value >>= 1

    syndrome = 0
    for _ in range(12):
        expected = 1 if (ecc & 0x800) else 0
        received = value & 1
        syndrome = (syndrome << 1) | (expected ^ received)
        ecc <<= 1
        value >>= 1

    return decoded, syndrome & 0x0FFF


def bits_lsb(codeword: int) -> list[int]:
    return [(codeword >> bit) & 1 for bit in range(30)]


def interleave_nine(codewords: list[int]) -> list[int]:
    assert len(codewords) == 9
    bits = [
        (codewords[column] >> row) & 1
        for row in range(30)
        for column in range(9)
    ]

    # Mirror rawbit(): each output codeword is rebuilt by consuming one bit
    # from every 9-symbol row, shifting right and inserting at bit 29.
    reconstructed: list[int] = []
    for column in range(9):
        value = 0
        for row in range(30):
            value >>= 1
            if bits[row * 9 + column]:
                value |= 0x20000000
        reconstructed.append(value)

    assert reconstructed == codewords
    return bits


def bits_to_symbols(bits: list[int]) -> list[int]:
    assert len(bits) % 2 == 0
    symbols: list[int] = []
    for index in range(0, len(bits), 2):
        pair = (bits[index] << 1) | bits[index + 1]
        symbols.append(PAIR_TO_SYMBOL[pair])
    return symbols


def symbol_bits(symbol: int) -> tuple[int, int]:
    assert 0 <= symbol <= 3
    return (
        1 if symbol > 1 else 0,
        1 if symbol in (1, 2) else 0,
    )


def build_sync_symbols() -> list[int]:
    bits = [(SYNC32 >> (31 - bit)) & 1 for bit in range(32)]
    symbols = bits_to_symbols(bits)
    assert len(symbols) == 16

    # Verify that the rolling ERMES::frame() sync register reaches SYNC32 only
    # on the final symbol, so this deterministic fixture cannot sync early.
    sync = 0
    distances: list[int] = []
    for symbol in symbols:
        first, second = symbol_bits(symbol)
        sync = ((sync << 1) | first) & 0xFFFFFFFF
        sync = ((sync << 1) | second) & 0xFFFFFFFF
        distances.append((sync ^ SYNC32).bit_count())

    assert all(distance >= 2 for distance in distances[:-1])
    assert distances[-1] == 0
    return symbols


def build_codewords() -> tuple[list[int], list[int]]:
    # First eight codewords are not interleaved. Codeword 3 is a non-zero
    # address-field word so APT at codeword 4 does not mark the batch empty.
    first_eight = [
        encode_data_codeword(0),  # block metadata
        encode_data_codeword(0),  # cycle/frame/batch = 0/0/A
        encode_data_codeword(0),
        encode_data_codeword(1),  # one address-field word -> nadd=1
        APT,
        encode_data_codeword(0),
        encode_data_codeword(0),
        encode_data_codeword(0),
    ]

    # showme() consumes the first 32 logical header bits as:
    #   [22-bit local address][remaining header bits]
    # The low two bits are paging category; category 0 is tone-only and AII
    # (bit 3) remains zero. Four trailing bits complete two 18-bit codewords.
    header32 = CAPCODE << 10
    header36 = header32 << 4
    header_word_1 = (header36 >> 18) & 0x3FFFF
    header_word_2 = header36 & 0x3FFFF
    assert header_word_1 == 0x1E24
    assert header_word_2 == 0

    interleaved = [
        MDEL,
        encode_data_codeword(header_word_1),
        encode_data_codeword(header_word_2),
        encode_data_codeword(0),
        encode_data_codeword(0),
        encode_data_codeword(0),
        encode_data_codeword(0),
        encode_data_codeword(0),
        encode_data_codeword(0),
    ]

    assert interleaved[1] == 0x1AD091E0
    return first_eight, interleaved


def build_symbols() -> bytes:
    first_eight, interleaved = build_codewords()

    raw_bits: list[int] = []
    for codeword in first_eight:
        raw_bits.extend(bits_lsb(codeword))
    assert len(raw_bits) == 240

    raw_bits.extend(interleave_nine(interleaved))
    assert len(raw_bits) == 510

    # Check the non-interleaved inverse exactly as rawbit() shifts it.
    reconstructed: list[int] = []
    for offset in range(0, 240, 30):
        value = 0
        for bit in raw_bits[offset:offset + 30]:
            value >>= 1
            if bit:
                value |= 0x20000000
        reconstructed.append(value)
    assert reconstructed == first_eight

    symbols = build_sync_symbols() + bits_to_symbols(raw_bits)
    assert len(symbols) == 271
    assert all(0 <= symbol <= 3 for symbol in symbols)
    return bytes(symbols)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUTPUT.bin", file=sys.stderr)
        return 2

    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)

    payload = build_symbols()
    digest = hashlib.sha256(payload).hexdigest()
    if digest != EXPECTED_SHA256:
        print(f"fixture SHA-256 mismatch: {digest}", file=sys.stderr)
        return 3

    output.write_bytes(payload)
    print(f"generated {output} ({len(payload)} symbols, sha256={digest})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
