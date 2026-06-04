# GOGUFW 0.5.11 F+9 Call Tone Menu Test 3

Changes:
- Adds main menu item `CLLTON` with 5 selectable call tones: TONE1..TONE5.
- Adds main menu item `CLLVOL` with LOW/MID/HIGH.
- When changing `CLLTON` with UP/DOWN inside the menu, the radio plays a short local preview only; it does not transmit.
- F+9 transmits the selected call tone for about 3.5 seconds.
- During this special call-tone transmission, the main screen TX label shows `CALL TX`.
- Normal PTT / normal TX display remains `TX`.
- Long-press 9 keeps the original 1-CALL channel shortcut.
- v0.5.11 Messenger config is migrated to preserve callsign/drafts/settings and add call-tone defaults.

Defaults:
- CLLTON: TONE1
- CLLVOL: MID

Build note:
- Docker is not available in this environment, so full firmware build was not run here.
- Host gcc syntax check passed for the modified C files; pointer-size warnings are expected from host gcc and are not firmware syntax errors.
