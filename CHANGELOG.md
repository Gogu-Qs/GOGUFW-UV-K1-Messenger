# Changelog

## GOGUFW 3.1.0 — 2026-10-10

- Ported Full Watch from F4HWN 6.1.0, including Full RX/Main TX modes, priority-channel rotation and Triple/Quad Watch display.
- Prevented the Full Watch middle-line indicator from overlapping the `UNLOCK KEYBOARD` notice; it is hidden for the notice duration and restored automatically.
- Removed Survival Mode and disabled Action Picker in the Fusion firmware profile.
- Merged Range Check into the HEARD screen. `WAIT` is drawn at the one-pixel left inset before PING transmission starts; existing MSG/ACK/PNG/PON rows remain visible, and PONG responders move to the top with evenly grouped voltage and RSSI details.
- Restored F4HWN 6.1.0's original Full Watch foreground/background pointer swap. A promoted RX channel remains visible, the displaced foreground stays in the rotation, and the normal cycle resumes after RX.
- Full Watch background-channel Messenger TEXT/PING RX now promotes only after a valid packet is decoded; ACK/PONG uses the promoted channel's actual RX/TX configuration and the normal Full Watch cycle resumes afterward.
- Fixed automatic ACK transmission from a promoted Full Watch priority channel: the ACK path now selects the actual promoted `VFO_Info` target rather than treating its shared A/B index as the physical foreground VFO, and restores the locked RX target after TX.
- Fixed manual PTT after a background message promotion in Full RX/Respond: Messenger voice-path cleanup now preserves the active received channel, while Full RX/Main TX continues to use the configured main TX VFO.
- Extended the A/B action in Full RX/Respond to cycle through every transmit-capable watched target (`A`, `B`, priority 1 and priority 2). Invalid or duplicate priority entries are skipped; Main TX/Full RX keeps its fixed-main-TX behavior.
- Extended the same watched-target selection to Main TX/Full RX; after a manual A/B action the displayed watched target becomes the PTT target, while automatic Full Watch reception keeps the configured main-TX behavior. Menu edits follow the displayed virtual priority VFO instead of silently editing physical B.
- Restored the overlay system-information capability and fixed the append-only app API layout. SystemInfo now receives the v6.1-compatible resident metadata, battery, storage and stack services.
- Cleared the middle display line immediately before drawing the RX RSSI meter, eliminating the first-frame overlap with the Triple/Quad Watch indicator.
- Updated the armed F-key status icon to the F4HWN 6.1.0 bitmap.
- Quad Watch battery saving now completes the full four-channel receive cycle before sleeping. Range Check explicitly releases its RF lock on EXIT/timeout, Full Watch restarts with a fresh 100 ms dwell after any Messenger lock, and an idle channel change can re-arm the Messenger sidecar in the same 10 ms pass. Long ACK/PONG waits intentionally continue to hold scanning.
- Integrated the applicable late F4HWN fixes: erased battery-calibration protection, the BK4829 134.4 Hz DCS tail-enable bit, Full Watch navigation after a priority swap, compact Cortex-M0+ signed division, packed small fonts, and centralized byte-setting menu descriptors.
- Confirmed that the scan-start priority-swap reset, selected-TX/current-VFO synchronization, packed large font, and overlay external-Flash asset API v2 were already present in GOGUFW and retained them unchanged.
- FM Radio now releases its audio path while Messenger handles an incoming or outgoing FSK frame, then restores the FM tuner, audio route and FM screen after ACK/PONG processing completes.
- Removed the analog voice Scrambler from the firmware configuration, radio UI and matching CHIRP controls; the legacy EEPROM byte is cleared and retained only for layout compatibility.
- Removed Compander from the Fusion UI and runtime. The legacy EEPROM field remains only to preserve storage compatibility.
- Removed Spectrum's VFO `AUTO` mode; VFO Spectrum now starts with one calibration pass and continues in manual mode.
- Centralized Full Watch target resolution in `RADIO_SelectVfos()`. Messenger, Search Tone, Spectrum, normal PTT, CALLTX and channel-menu edits now follow the actually displayed A/B/C/D target instead of a stale physical A/B slot.
- Added an exact physical-channel RF lock for Messenger. Background TEXT/PING replies remain on the channel that received the packet through the complete ACK/PONG exchange, then release the lock and resume Full Watch with a fresh 100 ms dwell.
- Added a 250 ms post-carrier Messenger receive hold so Triple/Quad Watch does not rotate away during the intentional gap between a WAKE frame and its TEXT/PING frame. ACK/PONG reception uses the same stable target.
- Messenger Compose, Inbox Read, Sent Read and HEARD now show `TO:` plus the selected memory name or VFO frequency, so send, reply, resend and Range Check destinations are visible.
- Fixed Search under Full Watch: opening the overlay no longer stalls the radio, and Frequency/Tone Search starts from the displayed A/B/C/D channel even after repeated watch rotations. The corrected app is released as `Search-v1.2-gogu.zip` and requires this firmware's corrected append-only API layout.
- Fixed the overlay API layout so optional resident SysInfo fields no longer shift later API entries between builds. This restores APRS Edit launch/config access and preserves compatibility for API-level 2/3 apps. The corrected APRS editor is released as `APRSEdit-message-only-v1.1-gogu.zip`.
- Added API level 4's temporary private-frequency service. The bundled custom APRS RX/TX app sources use it to start on 144.800 MHz and restore the user's A/B/C/D target on exit; unchanged upstream Armel APRS apps continue to use the selected radio channel.
- Updated firmware and CHIRP version identifiers to 3.1.0.

### Build information

- Firmware: `f4hwn.gogufw.v3.1.0.bin`
- Multiboot image: 120,636 bytes
- Firmware Flash: 119,712 / 120,832 bytes (1,120 bytes free before the 924-byte multiboot stub; 196 bytes free in the final image)
- RAM: 13,744 / 16,384 bytes (2,640 bytes free)

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
