#!/usr/bin/env python3
# APRS TX read-only assets: the screen texts and the fixed part of the frame.
#
# The UI texts come first, each padded to a multiple of 4 bytes: draw() reads
# them in one asset_read into a word-aligned stack block, so every text costs a
# 2-byte sp-relative add instead of a literal-pool load and a pool word.
# The frame parts are AX.25-encoded here (addresses shifted left, SSID bytes):
# the app adds the source (boot-message callsign + SSID), the path (the first 0,
# 1 or 2 entries of WIDE, end-of-address bit set by the app), the position and
# the FCS. SSID, PATH and the position are the defaults, until they are edited
# on the radio. Edit the station settings below, then rebuild;
# test/tx_model.py checks the resulting frame.
#
#   ./gen_assets.py aprstx_assets.bin aprstx_assets.h
import os, re, sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
from app_assets import Assets
from aprs_symbols_20 import BITMAPS, BYTES_PER_ICON, CODES, WIDTH

# ---- station settings ----
SSID = 7                                   # F4HWN-7 (0-15, 0 = no SSID)
DEST = "APZK5"                             # APZ = experimental software
PATH = 1                                   # index in PATHS below
WIDE = ["WIDE1-1", "WIDE2-1"]              # path n = the first n entries
PATHS = ["DIRECT", "WIDE1-1", "WIDE1-1,2-1"]   # shown on the radio, per path
LAT, LON = "4850.90N", "00216.25E"         # default position: DDMM.hhN, DDDMM.hhE
SYMBOL = "/["                              # table, code: '[' = person
COMMENT = "UV-K5/K1 F4HWN Firmware"
# info field: "!" + LAT + SYMBOL[0] + LON + SYMBOL[1] + COMMENT

TITLE = "APRS TX"
TX_CAPS = "TRANSMIT"                       # status-bar capsule while on the air

UI = [
    ("T_TITLE",  TITLE),
    ("T_TX",     TX_CAPS),
    ("T_DENIED", "TX denied"),
    ("T_NOCALL", "No boot callsign"),
    ("T_LVL",    "lvl "),
    ("T_TW",     "  tw "),
    ("T_SENT",   "  sent "),
    ("T_LAT",    "LAT  "),              # 2 spaces: digits aligned with LON
    ("T_LON",    "LON "),
    ("T_SSID",   "SSID "),              # FIELD_COL (5) characters: the value
    ("T_PATH",   "PATH "),              # starts at the same column
    ("T_SYM",    "SYMB "),              # selected APRS table/code pair
    ("T_HELP1",  "0-9, * change, F+* back"),        # * on the bold field
    ("T_HELP2",  "UP/DN move  MENU ok  EXIT"),
    ("T_BADPOS", "Invalid position"),
]


def addr(call, last=False):
    call, _, ssid = call.upper().partition("-")
    if not (1 <= len(call) <= 6 and call.isalnum()):
        sys.exit(f"bad AX.25 address {call!r}")
    b = bytes(ord(c) << 1 for c in call.ljust(6))
    return b + bytes([0x60 | (int(ssid or 0) << 1) | (1 if last else 0)])


if not 0 <= SSID <= 15:
    sys.exit("SSID must be 0-15")
if len(WIDE) != 2 or len(PATHS) != len(WIDE) + 1 or not 0 <= PATH < len(PATHS):
    sys.exit("PATHS lists DIRECT then 1 and 2 WIDE entries (frame buffer); PATH indexes it")
if any(len(s) > 18 - 5 for s in PATHS):
    sys.exit("a PATHS name does not fit the line after 'PATH '")
if not (re.fullmatch(r"\d{4}\.\d{2}[NS]", LAT) and re.fullmatch(r"\d{5}\.\d{2}[EW]", LON)):
    sys.exit("LAT must be DDMM.hhN/S, LON DDDMM.hhE/W")
if len(SYMBOL) != 2 or len(COMMENT) > 43:
    sys.exit("SYMBOL is 2 characters, COMMENT at most 43 (frame buffer)")
# 13 digits (DDMMhh DDDMMhh), then the hemispheres: bit 0 south, bit 1 west
POS = [int(c) for c in LAT + LON if c.isdigit()]
HEMI = (LAT[-1] == "S") | (LON[-1] == "W") << 1

a = Assets("APRSTX")
ui_size = 0
for name, s in UI:
    pad = -(len(s) + 1) % 4                 # keep every offset word-aligned
    a.text(name, s + "\0" * pad)
    ui_size += len(s) + 1 + pad
PATHS_STRIDE = -(-(max(len(s) for s in PATHS) + 1) // 4) * 4   # word-aligned entries
a.table("T_PATHS", PATHS, stride=PATHS_STRIDE)
ui_size += len(PATHS) * PATHS_STRIDE
a.const("UI_SIZE", ui_size)                 # the UI block read by draw()
a.const("T_TITLE_CHARS", len(TITLE))
a.const("T_TX_CHARS", len(TX_CAPS))
a.const("CFG_SSID", SSID)
a.const("CFG_PATH", PATH)
a.raw("F_DEST", addr(DEST))
a.raw("F_WIDE", b"".join(addr(h) for h in WIDE))
a.u8("POS_DEF", POS + [HEMI])
a.raw("F_COMMENT", COMMENT.encode("ascii"))
a.raw("SYM_CODES", "".join(CODES).encode("ascii"))
a.raw("SYM_BITMAPS", BITMAPS)
a.const("SYM_INDEX", CODES.index(SYMBOL))
a.const("SYM_COUNT", len(CODES))
a.const("SYM_W", WIDTH)
a.const("SYM_BYTES", BYTES_PER_ICON)
a.u8("BMP_F", [0x3e,0x7f,0x41,0x75,0x75,0x75,0x7d,0x7f,0x3e])   # F armed, as FoxHunt / FM / Beacon
# scroll marks (status bar, x = 75, as APRS RX): up only, down only, both; up
# in bits 0-2 (rows hidden above), down in bits 4-6 (rows hidden below)
UP, DN = [0x04, 0x06, 0x07, 0x06, 0x04], [0x10, 0x30, 0x70, 0x30, 0x10]
a.u8("BMP_SCROLL", UP + DN + [u | d for u, d in zip(UP, DN)])
a.const("BMP_SCROLL_W", 5)
a.main()
