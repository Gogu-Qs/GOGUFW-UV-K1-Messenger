# GGFW FM Names CHIRP support

This package keeps the firmware's real FM-name storage at PY25Q16 address `0x013000`.

Because the existing UV-Kx CHIRP serial read/write commands use 16-bit EEPROM-style offsets, the firmware adds a small EEPROM compatibility alias:

- CHIRP-visible alias: `0x00D000..0x00DFFF`
- Real flash storage: `0x013000..0x013FFF`

The CHIRP module reads/writes the alias only. The actual persistent names remain stored in the existing safe GGFW FM-name sector at `0x013000`.

Do not move FM names to `0x00F000`; that was a risky earlier idea and is not used here.
