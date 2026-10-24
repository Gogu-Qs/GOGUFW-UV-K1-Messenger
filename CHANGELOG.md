# Changelog

## GOGUFW 3.3.1 — 2026-10-25

### Breaking overlay API change

- Replaced GOGUFW's experimental public API levels 4–9 with the exact F4HWN
  6.3.0 ABI 1 / API 3 prefix. The official layout now includes number helpers,
  direct RX-audio capture, 48 MHz timing, app hand-off, quiet TX preparation,
  USB serial, paced LCD output and deferred VFO staging.
- Moved GOGU-only services into the separately versioned `APP_CAP_GOGU_EXT`
  table discovered through the official API-2 asset service. This leaves room
  for future F4HWN API levels without forcing another rewrite of every GOGU app.
- **Old GOGUFW overlay binaries built for experimental API levels 4–9 do not
  work with 3.3.1.** The loader deliberately reports them as incompatible.
  Reinstall `Search.app`, `FMVFO.app` and `FMMemory.app` from this release after
  flashing the firmware. Other custom GOGU apps also require their API-3 builds.

### Firmware and radio fixes

- Ported F4HWN 6.3.0 application services and added the serial capability used
  by the official KISS TNC app. KISS temporarily owns USB while running, then
  cleanly resynchronizes K5Viewer; CHIRP and Live Viewer remain available after
  exit. KISS starts on the frequency saved by APRS Edit/APRS TX and falls back
  to 144.800 MHz when the shared profile is absent or invalid.
- Fixed range scanning stopping on a detected carrier without opening audio
  when Messenger was enabled. The normal scan state machine now continues
  independently while Messenger still receives its radio interrupts.
- Compared the normal analog receive path with F4HWN 6.3.0. Restored the
  missing RX expander/TX compressor setup and limited the AM weak-signal filter
  behavior to AM instead of forcing it on normal FM channels. The upstream
  CTCSS/DCS found/lost and squelch state machine is retained.
- Restored GOGUFW's compact 20-entry RAM-only RF Log Lite. Removed the larger
  F4HWN RX/TX log, its external-Flash history and its UV Studio export protocol;
  generic K5Viewer/Live Viewer screen capture and CHIRP USB handling stay.
- Removed the resident SysInfo menu screen because diagnostics are supplied by
  `SystemInfo.app`. Its current stack value now follows the real stack pointer,
  and its minimum value uses a boot-time high-water pattern rather than the old
  fixed `_estack - _ebss` calculation.
- Preserved the 48-slot F4HWN 6.3.0 Labs identity used by UV Studio. APRS Edit
  and APRS RX use catalog-matching version metadata to avoid false warnings.
- Added ACARS RX fixed at 131.725 MHz and APRS-profile tuning for KISS TNC in
  source; the public release assets below remain limited to the requested core
  firmware, Search and paired Broadcast FM apps.

### Release files and build information

- Firmware: `GOGUFW-v3.3.1.bin`
- Multiboot firmware: `GOGUFW-v3.3.1-multiboot.bin`
- Required overlay updates: `Search.app`, `FMVFO.app`, `FMMemory.app`
- CHIRP module: `Gogufw_3.3.1_chirp_module.py`
- Final Flash image: 117,236 / 120,832 bytes (3,596 bytes free), including
  the 920-byte multiboot RAM-stub load image omitted by the linker region total
- RAM/linker reservation: 16,360 / 16,384 bytes; 4,096-byte runtime stack
  reservation retained and multiboot RAM-stub isolation verified

## GOGUFW 3.2.0 — 2026-10-21

- Rebuilt Broadcast FM as two paired overlays, `FMVFO.app` and
  `FMMemory.app`. Both are required. F+0 still opens the last-used FM mode and
  EXIT/F+0 returns to the exact radio screen and VFO/MR state that launched it.
- Preserved all GOGUFW FM features and visuals: four bands, manual and automatic
  scan, 48 named memories, save/delete, name editor and Live RSSI. Existing FM
  storage addresses and formats are unchanged.
- Fixed the held-3 hand-off leak between FM Memory and VFO, RSSI initialization
  after launch/mode change/RX resume, scan continuation, menu and Ch Save key
  routing, empty-band selection and the name editor's `*:B`/`*:b`/`*:2` label.
- During selected-channel RX, FM yields to the main screen and radio audio, then
  restores the same FM mode and tuning five seconds after RX ends. EXIT remains
  usable during continuous RX.
- Fixed missed squelch-close recovery on modal screens, preventing RX and audio
  from remaining latched while browsing Apps or editing FM data.
- Removed the resident FM UI/state machine and replaced the 768-byte FM-name
  staging table with a safe single deferred record. Firmware Flash use is
  114,020 / 120,832 bytes, saving 5,888 bytes versus 3.1.1.
- Enforced a 4 KiB minimum runtime stack at link time. The final image has a
  4,148-byte stack gap, restoring APRS RX operation and preventing silent stack
  overlap from future resident-RAM growth.
- Restored the per-channel Scrambler and Compander firmware controls.
- Added the matching `Gogufw_3.2.0_chirp_module.py`. It retains the 3.1.1
  FrozenMemory/invalid-value regression fix and exposes Scrambler, Compander,
  No FSK TX and No Roger using the unchanged EEPROM fields.
- Published Search v1.5 with the release. Its Frequency and Tone status/result
  rows are vertically centred between the dotted separators; install it to
  enable the F+4 and F+STAR shortcuts.

### Build information

- Firmware: `f4hwn.gogufw.v3.2.0.bin`
- Multiboot image: 114,940 bytes (5,892 bytes free)
- Firmware Flash before multiboot stub: 114,020 / 120,832 bytes (6,812 bytes free)
- Runtime stack gap: 4,148 bytes (4,096-byte minimum enforced)
- FM VFO overlay: 4,092 / 4,096 bytes
- FM Memory overlay: 4,072 / 4,096 bytes
- Search overlay: 3,060 / 4,096 bytes

## GOGUFW 3.1.1 — 2026-10-17

- Removed Rescue Ops and the 1750 Hz call tone from the Fusion build, retained Screenshot support, and switched LTO to a single optimization partition for lower Flash usage without changing runtime features.
- Fixed Full Watch VFO/MR switching while priority channel C or D is selected. V/M now changes the selected C/D object's temporary mode in place without jumping to or modifying its physical A/B host; A/B/C/D navigation preserves that mode until the priority channel is reloaded.
- Reclaimed 576 bytes of overlay-app runtime stack without removing firmware features by allocating Spectrum's channel bitmap, RSSI history and peak-hold arrays only while Spectrum owns the foreground. This fixes APRS RX launch failure in both the bundled and original Armel packages while leaving Spectrum's own effective memory budget unchanged.
- Fixed overlay launch preparation and cleanup retaining an oversized live stack frame while an app was running. The safe split-phase launcher leaves only a 40-byte dispatcher frame alive and never moves the stack over suspended caller data.
- Apps re-arm the backlight when closing, and settings loading repairs a persisted invalid `BLMax=0` to 10.
- Restricted the middle-row pre-RSSI cleanup to Full Watch. Triple/Quad Watch still clears its center indicator before drawing the RSSI meter, while Main Only RX no longer erases unrelated middle-screen content when its RSSI meter is drawn on the lower row.
- Verified the late F4HWN erased packed-settings-byte correction is already present in GOGUFW. Erased FM/AM audio-profile data is stored separately and already falls back safely to profile 0, so no audio profile is removed or loaded from an erased value.
- Renamed the Messenger `MsgCsg` menu item to `CllSgn` and moved it to the end of the Messenger settings immediately before `RngRsp`. Compatible APRS TX and Beacon apps now use this shared station callsign, falling back to the unchanged Power On Message only when `CllSgn` is empty or invalid.
- Updated firmware and CHIRP version identifiers to 3.1.1.
- Restored the complete deferred Broadcast FM overlay bridge when the resident FM radio is enabled. Armel's original Broadcast FM app now receives real BK1080/state/save callbacks; GOGUFW's resident FM names, live-RSSI interface and Messenger return flow remain unchanged.
- Added the bounded configuration-proxy capability used by current Armel APRS Edit/TX packages, extended it for GOGUFW's 122-byte APRS profile, and added saved-screen persistence for current SSTV packages.
- Corrected overlay shortcut parsing for current single-ID headers. GOGUFW Search's combined ID still owns F+4/F+STAR regardless of slot order, while unrelated IDs can no longer be mistaken for multiple bit-mask shortcuts.
- Verified the archived Armel v6.1.0 and v6.2.0 app sets against the final loader headers, VMA, CRCs and capability mask. Beam remains intentionally unsupported because its dedicated resident FSK bridge is disabled; all other archived apps validate with their required services.
- Ported the applicable F4HWN 6.2.0 reliability fixes: the automatic keypad-lock countdown is now wide enough for every menu duration, S-meter band 1/2 corrections are preserved on settings saves and repaired after the historical zeroing bug, and the K1 battery entry is correctly labelled 1400 mAh.
- Expanded the external-Flash app region from 16 to 48 slots. Empty slots remain hidden; held UP/DOWN repeats through long lists, and `*` toggles the right column between code size and app version. The loader, UART/USB management and config-proxy lookup all use the same 48-slot limit.
- Fixed UV Studio still reporting `16 of 16 slots used`: the host-only UART identity now advertises the compatible F4HWN 6.2 baseline first, allowing UV Studio to select its 48-slot layout. The radio UI and SysInfo continue to show GOGUFW 3.1.1.
- Suppressed the false main-channel RX indication caused by a manual Broadcast FM retune on frequency-sensitive VFO combinations such as 439.400 MHz. The guard follows the BK4829 squelch latch instead of expiring while its stale open event is still active, with a bounded timeout for a real carrier.

Only the firmware image and matching CHIRP module are published as 3.1.1 release assets. Personal GOGUFW-modified overlay-app packages are not included.

### Build information

- Firmware: `f4hwn.gogufw.v3.1.1.bin`
- Multiboot image: 120,828 bytes
- Firmware Flash: 119,908 / 120,832 bytes (924 bytes free before the 920-byte multiboot stub; 4 bytes free in the final image)
- RAM: 13,288 / 16,384 bytes (3,096 bytes free after the linker reservation; the overlay launcher's live frame while app code runs is reduced from 224 to 40 bytes without moving the stack)

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
