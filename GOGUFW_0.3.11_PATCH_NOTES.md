# GOGUFW 0.3.11 Patch Notes

- Reworked `MsgCsg` callsign editing to reuse the existing F4HWN `ChName` editor flow.
- Callsign remains limited to 6 characters.
- Preserved the 0.3.4 EEPROM storage hotfix: Messenger config uses private flash sector `0x012000`, not EEPROM compatibility address `0x1E80`.
