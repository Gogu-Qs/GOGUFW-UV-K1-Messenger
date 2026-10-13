# APRS Edit

APRS Edit owns the APRS TX comment and station settings: position, SSID, path
and symbol. It preserves APRS TX's tone level, twist and display mode.

APRS TX key `3` opens APRS Edit. When launched this way, both `MENU` (save) and
`EXIT` (discard) return to APRS TX. When APRS Edit is launched directly from the
Apps menu, both actions return to the Apps menu.

- `PTT`: switch between the message and station-settings pages
- `MENU`: validate, save and leave the app
- `EXIT`: discard changes and leave the app
- Message page: `*` changes T9 mode; `F` deletes; holding `F` repeats deletion;
  long-pressing `0`-`9` inserts the digit
- Settings page: `UP/DOWN` moves the cursor; digits edit coordinates; `*`
  changes the selected hemisphere/SSID/path/symbol; `F`, then `*`, changes the
  selected value backwards

The APRS TX app must be installed for shared settings to be saved.
