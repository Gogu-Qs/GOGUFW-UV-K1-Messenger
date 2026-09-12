# GOGUFW 2.0.0

GOGUFW 2.0.0 adds official F4HWN 6.0.0-compatible multiboot and multiconfig support while preserving Messenger, HEARD, Range Check, CALLTX, FM tools and Memory Spectrum.

## Multiboot and configuration banks

- Runs as the Main firmware or from a compatible firmware slot.
- Hold **MENU while powering on** to open the firmware-slot selector.
- Supports UV Studio Firmware Slots operations: inspect, write, validate and erase slots, plus reset configuration banks.
- Fully validates a slot image before replacing the internal application image.
- Uses redundant boot metadata for safer recovery from an interrupted update.
- Gives each firmware slot its own settings bank by default, including GOGUFW Messenger data and FM station names.
- Adds **SetCfg** for deliberately sharing a compatible settings bank. A normal reboot keeps the selection; switching to another firmware and later returning restores the slot's own bank as a safety measure.
- Reports the firmware version as **v2.0.0** in multiboot tools.

## Messenger reliability and compatibility

- Keeps power save suspended during active FSK receive, ACK and Range Check transactions.
- Holds Dual Watch on the correct receive VFO across the wake preamble and message packet.
- Keeps the complete 12-second PONG collection window active while showing incoming Range Check results immediately.
- Preserves the existing packet, ACK, retry, PING/PONG and callsign formats for compatibility with existing GOGUFW radios.

## UI fixes

- Callsign and channel-name editors now allow the same keypad character to be entered again after the multi-tap timeout.
- Channel-name editing saves and exits with a single MENU press.
- Keeps the v1.3.0 Memory Spectrum behavior, safe second-PTT channel/frequency selection and normal-screen startup fix.

## Memory balance

Beam, QR display, MEM display, Scan RSSI and Scan Progress were removed to make room for multiboot while retaining the core GOGUFW features.

- FLASH: **116,932 / 120,832 bytes (96.77%)**
- RAM: **13,408 / 16,384 bytes (81.84%)**

## Downloads

- `f4hwn.gogufw.v2.0.0.bin` — firmware image with canonical UV Studio/multiboot naming
- `Gogufw_2.0.0_chirp_module.py` — matching CHIRP module under Quansheng

## Hardware warning

This firmware is only for Quansheng **UV-K1 / UV-K5 V3** radios using the **PY32F071 MCU and BK4829 RF IC**. Do not install it on UV-K5 V1/V2 or other BK4819-based models.

Back up the radio with CHIRP before first using multiboot. During the first Main initialization, do not switch off the radio while `Init Main` is displayed.
