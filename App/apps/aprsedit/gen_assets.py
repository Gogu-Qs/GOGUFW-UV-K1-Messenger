#!/usr/bin/env python3
import os,sys
sys.dont_write_bytecode=True
HERE=os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0,os.path.join(HERE,".."))
from app_assets import Assets
from aprs_symbols_20 import CODES
a=Assets("APRSEDIT")
a.raw("SYM_CODES","".join(CODES).encode("ascii"))
a.u8("POS_DEFAULT",[int(c) for c in "4850900021625"])
a.const("HEMI_DEFAULT",0)
a.const("SYM_DEFAULT",CODES.index("/["))
a.const("SYM_COUNT",len(CODES))
a.main()
