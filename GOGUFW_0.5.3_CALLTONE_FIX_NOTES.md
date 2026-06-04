# GOGUFW 0.5.11 Call Tone / Storage Fix

- F+9 sends selected PMR-style call tone.
- Long-press 9 keeps original 1 CALL channel shortcut.
- Added CllTon and CllVol menu entries under SetScn and above MsgRx.
- CllTon preview is non-blocking and can be interrupted by UP/DOWN/MENU/EXIT.
- CllVol affects RF call-tone intensity using tone gain plus duty/spacing.
- CALLTX label is used during call-tone TX only; normal TX remains unchanged.
- Messenger config layout fixed: CllTon/CllVol are appended at the end to avoid shifting MsgID/callsign/drafts.
- Includes recovery path for the temporary bad v5 test layout; already overwritten first bytes cannot always be reconstructed, but further offset damage is stopped.
- Visible version strings updated to 0.5.11.
