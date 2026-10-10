# APRS Edit

APRS Edit changes only the 23-byte comment embedded in the original F4HWN
`APRS TX` overlay. APRS TX itself stays unmodified; its position, SSID, path and
symbol editor continues to work exactly as documented upstream.

- `MENU`: save the message and leave the app
- `EXIT`: leave without saving
- `*`: switch T9 mode (`B`, `b`, `2`)
- `F`: delete the last character
- long press `0`-`9`: insert that digit in letter mode

Short messages are space-padded to the original asset's fixed 23-byte length.
Install the original `APRS TX` app before saving a message. Reinstalling APRS TX
restores its built-in comment, which can then be changed again with APRS Edit.
