#!/usr/bin/env python3
# Broadcast FM read-only assets: texts, band names, the F icon and the
# decimal places.
#
#   ./gen_assets.py fm_assets.bin fm_assets.h
import os, sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from app_assets import Assets

a = Assets("FM")
a.text("T_FM", "FM")
a.text("T_FM_RADIO", "FM RADIO")
a.text("T_SAVE", "SAVE?")
a.text("T_DEL", "DEL?")
a.text("T_ASCAN", "A-SCAN(")
a.text("T_MSCAN", "M-SCAN")
a.text("T_MR_CH", "MR(CH")
a.text("T_VFO", "VFO")
a.text("T_VFO_CH", "VFO(CH")
a.text("T_CH", "CH-")
a.text("T_LIVE_RSSI", "LIVE RSSI")
a.text("T_CH_NAME", "CH-NAME")
a.text("T_CH_DEL", "CH-DEL?")
a.text("T_ON", "ON")
a.text("T_OFF", "OFF")
a.text("T_AUTOSCAN", "AUTO SCAN?")
a.text("T_CONFIRM", "MENU=YES EXIT=NO")
a.text("T_NAME", "NAME")
a.text("T_SCAN", "SCAN")
a.text("T_MENU", "MENU")
a.text("T_EXIT", "EXIT")
a.text("T_UPDN", "UP/DN")
a.text("T_SAVED_CHANNELS", "SAVED CHANNELS")
a.text("T_WILL_BE_ERASED", "WILL BE ERASED")
a.text("T_SURE", "SURE?")
a.text("T_F_DEL", "F:DEL")
a.text("T_LIVE", "LIVE")
a.table("BAND_NAME", ["87.5-108M", "76-108M", "76-90M", "64-76M"])   # by band 0..3
a.u8("BMP_F", [0x3e,0x7f,0x41,0x75,0x75,0x75,0x7d,0x7f,0x3e])
a.u16("PLACE", [1000, 100, 10, 1])    # decimal places: numbers without division
a.main()
