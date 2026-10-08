"""Reference encoder/decoder for ngpc_qr_result (OVER REV's OV1 layout).

    python result_codec.py --self-test
    python result_codec.py "HTTPS://EXAMPLE.COM/MYGAME/MG1:ACE:..."   # decode

Text: <URL><TAG>:<NAME>:<BASE32(data + CRC16 big-endian)>
CRC-16/CCITT-FALSE over ASCII "<TAG>:<NAME>:" then data. The URL is not hashed.
A valid CRC means consistent data, NOT an authentic performance.
"""
from __future__ import annotations
import binascii
import re
import sys

B32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"
PAYLOAD = re.compile(r"([A-Z0-9]{1,8}):([A-Z0-9]{1,16}):([A-Z2-7]+)$")


def crc16(data: bytes) -> int:
    return binascii.crc_hqx(data, 0xFFFF)


def b32encode(data: bytes) -> str:
    acc = bits = 0
    out = []
    for byte in data:
        acc = (acc << 8) | byte
        bits += 8
        while bits >= 5:
            bits -= 5
            out.append(B32[(acc >> bits) & 31])
        acc &= (1 << bits) - 1
    if bits:
        out.append(B32[(acc << (5 - bits)) & 31])
    return "".join(out)


def b32decode(text: str) -> bytes:
    acc = bits = 0
    out = bytearray()
    for ch in text:
        acc = (acc << 5) | B32.index(ch)
        bits += 5
        if bits >= 8:
            bits -= 8
            out.append((acc >> bits) & 255)
            acc &= (1 << bits) - 1
    if acc:                      # padding bits must be zero
        raise ValueError("non-zero padding bits")
    return bytes(out)


def encode(url: str, tag: str, name: str, data: bytes) -> str:
    head = f"{tag}:{name}:"
    crc = crc16(head.encode("ascii") + data)
    return url + head + b32encode(data + bytes([crc >> 8, crc & 255]))


def decode(text: str, url: str = "") -> tuple[str, str, bytes]:
    """Return (tag, name, data). `url` is the exact prefix your game prints;
    the payload is case-sensitive (covered by the CRC): never upper-case it."""
    if url:
        if not text.startswith(url):
            raise ValueError("unexpected URL prefix")
        text = text[len(url):]
    m = PAYLOAD.fullmatch(text)
    if not m:
        raise ValueError("not a TAG:NAME:BASE32 payload")
    tag, name, b = m.groups()
    raw = b32decode(b)
    if len(raw) < 3 or b32encode(raw) != b:
        raise ValueError("bad Base32 length")
    data, stored = raw[:-2], raw[-2] << 8 | raw[-1]
    if crc16(f"{tag}:{name}:".encode("ascii") + data) != stored:
        raise ValueError("CRC mismatch (damaged code)")
    return tag, name, data


# OV1 vector, same bytes as OVER REV's protocol example (neutral name): rules 1, circuit 0, car 0, tune 0,
# Time Attack + AT, 4500 ticks, competition 0, sequence 1.
OV1_DATA = bytes([0, 1, 0, 0, 0, 0x11, 0, 0x11, 0x94, 0, 0, 0, 1])
OV1_TEXT = "OV1:PLAYER01:AAAQAAAACEABDFAAAAAADYBN"


def self_test() -> None:
    assert crc16(b"123456789") == 0x29B1
    assert encode("", "OV1", "PLAYER01", OV1_DATA) == OV1_TEXT
    assert decode(OV1_TEXT) == ("OV1", "PLAYER01", OV1_DATA)
    url = "HTTPS://WWW.NGPC-DEV.COM/OVERREV/"
    assert decode(url + OV1_TEXT, url)[2] == OV1_DATA
    for n in range(1, 33):
        d = bytes((i * 37 + n) & 255 for i in range(n))
        t = encode("", "T1", "N", d)
        assert decode(t) == ("T1", "N", d), n
    bad = OV1_TEXT[:-1] + ("A" if OV1_TEXT[-1] != "A" else "B")
    try:
        decode(bad)
    except ValueError:
        pass
    else:
        raise AssertionError("damaged code accepted")
    print("self-test OK (OV1 vector, 32 lengths, damage rejected)")


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-test"]:
        self_test()
    elif len(sys.argv) == 2:
        print(decode(sys.argv[1].split("/")[-1]))
    else:
        sys.exit(__doc__)
