# GGFW RF31 - VFO-aware ACK + 0.1.7 branding

Base: RF30 callsign/read UI.

Changes:
- ACK now remembers the RX VFO on which the message was decoded.
- ACK TX is sent on that RX VFO, then the previous user-selected TX VFO is restored.
- This targets DWR/DW cases where a message arrives on A or B but the active main TX VFO is the other side.
- Manual message TX behavior is unchanged: it still uses the currently selected TX VFO.
- Branding/version defaults updated to GOGUFW 0.1.7 via CMake and VS Code/CMake presets.

Safety rules preserved:
- REG_47 is not changed for FSK RX.
- RF30 ACK/text/retry and UI behavior are otherwise unchanged.
