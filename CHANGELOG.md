# Changelog

## GOGUFW 2.3.3 — 2026-09-30

This release contains the changes made since GOGUFW 2.3.1. Version 2.3.2 was an internal development version and was not published as a stable release.

### FM radio and CHIRP

- Fixed FM memories created with CHIRP not appearing in FM memory mode after **Reset All** or on a fresh installation. The matching 2.3.3 CHIRP module now initializes an erased or inconsistent FM selected-frequency, selected-channel and band state from the uploaded FM memories while preserving a valid existing state.
- Fixed empty FM station-name fields being difficult or impossible to edit in CHIRP by disabling automatic space padding in the editor and applying the required flash padding only when saving.
- Preserved an already-valid FM-name header instead of reinitializing it for every edited name.
- During FM VFO scanning, the RSSI area continues to show five dashes while the tuner is stepping, then samples the found station at least once. With **LIVE RSSI** enabled, the meter continues to update while stopped on that station.
- Aligned the FM RSSI meter and `LIVE` label at the same one-pixel left inset.
- Corrected the right alignment of the `108.0` FM scale label.

### Keys and audio feedback

- Fixed locked **MENU** handling: the `UNLOCK KEYBOARD` notice now appears on the initial press, and holding MENU can no longer execute its assigned long-press action while the keypad is locked.
- Removed the duplicate optional beep that occurred when opening Messenger with **F + MENU**.
- On FM Radio, Messenger Home and HEARD/idle Range Check, **EXIT** now clears an active **F** state first; a second EXIT leaves the screen.

### Interface

- Matched the `TX BLOCKED` title typography and popup alignment to the `UNLOCK KEYBOARD` notice while retaining the smaller reason text.
- Vertically centred empty-state text in HEARD, Range Check, Inbox, Sent and Drafts.
- Moved the FSK icon one pixel left and the Roger icon one pixel right to improve spacing on the main radio screen.
- Inset custom-screen header counters, channel numbers, editor mode indicators and delivery markers by one pixel and aligned their baseline with the centred title.

### Radio compatibility

- Corrected BK4829 Scrambler enable/disable handling to match the known-good Sonic implementation and avoid modifying register `0x2B` unnecessarily.
- Messenger framing, message IDs, wake-preamble policy, ACK/retry behavior, ping/pong format and persistent Messenger data layout are unchanged from 2.3.1.

### Release files

- Firmware: `f4hwn.gogufw.v2.3.3.bin`
- CHIRP module: `Gogufw_2.3.3_chirp_module.py`
- Multiboot image size: 119,996 / 120,832 bytes (836 bytes free)
- Firmware FLASH usage: 119,076 / 120,832 bytes
- RAM usage: 13,520 / 16,384 bytes
