# APRS Edit

APRS Edit owns the APRS TX comment and station settings: position, SSID, path
and symbol. It preserves APRS TX's tone level, twist and display mode.

APRS TX key `3` opens APRS Edit's two-item menu: `APRS MESSAGE` and `SETTINGS`.
`MENU` selects an item. Inside either editor, `MENU` saves that page and returns
to the two-item menu; `EXIT` discards that page and returns to the same menu.
Pressing `EXIT` once more leaves APRS Edit: it returns to APRS TX when launched
from there, or to the Apps menu when launched directly.

- Main menu: `UP/DOWN` selects; `MENU` opens; `EXIT` leaves APRS Edit
- Editor page: `MENU` saves and returns; `EXIT` discards and returns
- Message page: `*` changes T9 mode; `F` deletes; holding `F` repeats deletion;
  long-pressing `0`-`9` inserts the digit
- Settings page: `UP/DOWN` moves the cursor; digits edit coordinates; `*`
  changes the selected hemisphere/SSID/path/symbol; `F`, then `*`, changes the
  selected value backwards

The APRS TX app must be installed for shared settings to be saved.
