#!/usr/bin/env python3
"""Build first-generation 406 MHz beacon frames (C/S T.001) as hex, for tests.

Default: the reference frame from the beacon generator used on the bench, a long
standard location test protocol message (country 227, test data 0x123456,
position 49 deg 16'16" N, 0 deg 46'56" E, external source, 121.5 homing),
15-hex ID 1C7C2468ACFFBFF.

  frame406.py                 long normal frame   (144 bits, 36 hex)
  frame406.py --selftest      same message, self-test frame sync
  frame406.py --short         short message       (112 bits, 28 hex)
  frame406.py --flip N        invert message bit N (25..144), BCH must fail
"""
import argparse

G1 = 0b1001101101100111100011   # x^21+x^18+x^17+x^15+x^14+x^12+x^11+x^8+x^7+x^6+x^5+x+1
G2 = 0b1010100111001            # x^12+x^10+x^8+x^5+x^4+x^3+1
SYNC_NORMAL = [0, 0, 0, 1, 0, 1, 1, 1, 1]
SYNC_SELFTEST = [0, 1, 1, 0, 1, 0, 0, 0, 0]


def bits(v, n):
    return [(v >> (n - 1 - i)) & 1 for i in range(n)]


def bch(data, gen, r):
    reg = 0
    for b in data + [0] * r:
        reg = (reg << 1) | b
        if reg >> r:
            reg ^= gen
    return bits(reg, r)


def build(short=False, selftest=False, flip=None):
    country, proto, data = 227, 0b1110, 0x123456
    lat_c = [0] + bits(49, 7) + bits(1, 2)      # N, 49 deg, 1 x 15'
    lon_c = [0] + bits(0, 8) + bits(3, 2)       # E, 0 deg, 3 x 15'
    pdf1 = [0 if short else 1, 0] + bits(country, 10) + bits(proto, 4) + bits(data, 24) + lat_c + lon_c
    msg = pdf1 + bch(pdf1, G1, 21)              # bits 25..106
    if short:
        msg += [0] * 6                          # bits 107..112, national use
    else:
        pdf2 = [1, 1, 0, 1] + [0] + [1] \
            + [1] + bits(1, 5) + bits(16 // 4, 4) + [1] + bits(1, 5) + bits(56 // 4, 4)
        msg += pdf2 + bch(pdf2, G2, 12)         # bits 107..144
    if flip is not None:
        msg[flip - 25] ^= 1
    frame = [1] * 15 + (SYNC_SELFTEST if selftest else SYNC_NORMAL) + msg
    return "%0*X" % (len(frame) // 4, int("".join(map(str, frame)), 2))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--short", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--flip", type=int)
    a = ap.parse_args()
    print(build(a.short, a.selftest, a.flip))
