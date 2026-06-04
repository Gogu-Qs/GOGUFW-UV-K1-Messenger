# GOGUFW 0.3.4 patch notes

Base: GOGUFW 0.3.3

## Critical storage fix

- Moved Messenger persistent configuration storage out of the EEPROM compatibility / MR channel memory area.
- Old risky location: EEPROM-compatible address `0x1E80`.
- New location: dedicated external flash sector `0x012000`.
- This avoids possible overwrite/corruption of user memory channels.

## Storage map decision

Current verified external flash usage in source:

- `0x000000–0x003FFF`: MR frequency sectors
- `0x004000–0x007FFF`: MR name sectors
- `0x008000–0x00886E`: MR/VFO attributes and list names
- `0x009000–0x0090D6`: VFO settings
- `0x00A000–0x00A170`: radio/settings/FM/settings-version sector
- `0x010000–0x0101FF`: calibration area
- `0x011000–0x011FFF`: boot logo sector
- `0x012000–0x012FFF`: GOGUFW Messenger private config sector

## Note

Existing Messenger settings stored by 0.3.3 at `0x1E80` are intentionally not migrated automatically, because that address overlaps the channel-memory compatibility area. After flashing 0.3.4, Messenger settings may reset to defaults and should be reconfigured once.
