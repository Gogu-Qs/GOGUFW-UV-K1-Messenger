#!/usr/bin/env python3
# EPIRB 406 read-only assets: the screen texts and the protocol names.
#
# The UI texts come first, each padded to a multiple of 4 bytes: draw() reads
# them in one asset_read into a word-aligned stack block, so every text costs a
# 2-byte sp-relative add instead of a literal-pool load and a pool word.
# The protocol names are parsed from dec406.c (NAMES in dec406_proto_name), so
# the host test and the app share one table.
#
#   APP_VER=1.6 ./gen_assets.py epirb406_assets.bin epirb406_assets.h
import os, re, sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
from app_assets import Assets

def proto_names():
    src = open(os.path.join(HERE, "dec406.c")).read()
    m = re.search(r"static const char NAMES\[\]\s*=(.*?);", src, re.S)
    if not m:
        sys.exit("dec406.c: NAMES table not found")
    packed = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))
    names = packed.split("\\0")
    if len(names) != 24:
        sys.exit(f"dec406.c: {len(names)} protocol names, 24 expected")
    return names

TITLE = "EPIRB 406 v" + os.environ.get("APP_VER", "dev")

UI = [
    ("T_TITLE",    TITLE),
    ("T_WAIT",     "Waiting..."),
    ("T_NOPOS",    "no position"),
    ("T_SELFTEST", "SELF-TEST "),
    ("T_LONG",     "LONG"),
    ("T_SHORT",    "SHORT"),
    ("T_BCH",      " BCH "),
    ("T_OK",       "OK"),
    ("T_ERR",      "ERR"),
    ("T_DBM",      "dBm"),
    ("T_INT",      " int"),
    ("T_EXT",      " ext"),
    ("T_HOMING",   " 121.5"),
    ("T_COARSE",   " coarse"),
    ("T_RAWID",    " rawID"),
    ("T_KHZ",      " kHz"),
    ("T_DBMOK",    "dBm ok"),
    ("T_ERRS",     " err"),
    ("T_NOSYNC",   " nosync"),
    ("T_CUT",      " cut"),
]

a = Assets("EPIRB406")
ui_size = 0
for name, s in UI:
    pad = -(len(s) + 1) % 4                 # keep every offset word-aligned
    a.text(name, s + "\0" * pad)
    ui_size += len(s) + 1 + pad
a.const("UI_SIZE", ui_size)                 # the UI block read by draw()
a.const("T_TITLE_CHARS", len(TITLE))
a.table("T_PROTO", proto_names())           # 16 location, then 8 user protocols
a.main()
