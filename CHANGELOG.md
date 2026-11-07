# Changelog

## GOGUFW 3.6.2 — 2026-11-07

### Scrambler

- Restored the per-channel scrambler setting in the GOGUFW Fusion build and
  made channel changes retain it correctly. Returning to a channel now restores
  that channel's saved scrambler state instead of silently switching it off.
- Applied the selected scrambler consistently to normal FM receive and voice
  transmit paths. DTMF, FSK, alarm/tone, CSS-tail and Roger/MDC signalling are
  explicitly sent without voice inversion, then normal voice operation restores
  the channel setting.
- Added a compact uppercase `S` in the smallest status font between the No FSK
  and No Roger indicators. The FSK indicator now starts at x=111 so the three
  indicators remain tightly spaced without overlapping.

### Overlay power save

- Confirmed that overlay applications do not enter power save while they own
  the main loop. The idle timer can still expire in the background, however,
  leaving a pending power-save request behind.
- Reset the idle countdown and clear pending/expired power-save requests when
  an overlay closes. Returning from an app is now treated as user activity, so
  the radio no longer enters power save immediately or flashes the `Zz` status
  icon as soon as the normal screen returns.

### Compatibility and release files

- Existing overlay applications remain compatible and were not updated for
  this firmware-only release. The previous application versions may continue
  to be used.
- `Apps.zip` is supplied for convenience and contains the unchanged
  `FMVFO.app`, `FMMemory.app` and `Search.app` files.
- Updated the CHIRP module's driver, variant and firmware-comparison identifiers
  to 3.6.2. Its channel/settings schema is unchanged from the 3.6.0 module.
- Firmware: `GOGUFW-v3.6.2.bin` (single image for direct flashing and the
  supported multiboot workflow).
- CHIRP module: `Gogufw_3.6.2_chirp_module.py`.
- Final published image: **120,220 / 120,832 bytes** (612 bytes free), including
  the 920-byte multiboot load image. Linked code/data uses 119,300 bytes.
- RAM with the enforced stack reservation: **15,832 / 16,384 bytes** (552 bytes
  free); the 4,096-byte runtime stack and multiboot RAM-stub isolation checks pass.

## GOGUFW 3.6.0 — 2026-11-04

### Spectrum

- Expanded VFO Spectrum from the original 16/32/64/128 sample choices to
  **16/32/64/128/256/512/1024/2048 samples**. Higher values cover a much wider
  frequency span without requiring one LCD column for every RF sample; multiple
  samples are combined into the 128-column graph.
- Corrected centred VFO-window arithmetic and band-edge clamping for the new
  spans. Changing the sample count, entering a frequency or launching Spectrum
  near a band edge no longer shifts, wraps or truncates the requested range.
- Kept the established pre-expansion graph/ruler height in both VFO and Memory
  Spectrum. Memory channel labels fit around the unchanged ruler instead of
  pushing the graph upward.
- Reduced ruler density at 512/1024/2048 samples and clears only the small
  marker field before drawing the selected-peak triangle. Wide-span rulers no
  longer turn into a solid comb or obscure the marker.
- Fixed the temporary `M---` value that could remain after listening to a
  signal. The first valid RSSI measurement now refreshes the status line
  immediately.
- Replaced Spectrum's private battery drawing with the exact normal-radio
  battery icon and low-battery blink behaviour. VFO channel-name refreshes no
  longer erase the battery's left edge and make it flash.

### Overlay API and F4HWN 7.0 compatibility

- Advanced the public overlay contract from official **ABI 1 / API 3** to the
  append-only **F4HWN API 4** layout. It adds memory-channel/list access and
  staging, per-app TX-power control and the GPS-position service slot. The
  existing API-3 prefix is byte-for-byte unchanged, so applications built for
  GOGUFW 3.3.1–3.3.5 remain compatible.
- Added API-4 memory staging: a compatible overlay can select a stored channel,
  return to the main radio and have the new VFO/MR state committed safely after
  the overlay has closed.
- Added the official GPS entry point without inventing a stale fix. Until a live
  K5Viewer GPS position is retained by the firmware, it returns unavailable so
  applications can use their saved-coordinate fallback.
- Extended the separately versioned GOGU capability table with resident analog
  voice PTT and shared F/key-lock state. This does not consume unofficial public
  API numbers and leaves future F4HWN levels free to append normally.
- Corrected overlay analog voice TX to enter the same resident transmit/audio
  path as physical PTT, including decoder reset, audio filtering, PTT-ID policy
  and TX ownership. Overlay exit also restores the selected VFO's squelch and
  output-power state.
- Added the F4HWN 7.0 `SetRxG` option for the BK4829 receive audio gate. RX
  Compander forces the required gate automatically; otherwise the user may
  disable it to pass weaker modulation at the cost of more inter-word hiss.
- Corrected BK4829 Compander handling. BK4819 ratio-table writes are no longer
  sent to BK4829 noise-gate/HPF registers, preventing muted or damaged ordinary
  analogue RX audio.

### FM VFO and FM Memory overlays

- Updated both paired Broadcast FM applications to **3.6.0-gogu**. The resident
  status bar is now continuous from launch and remains visible while tuning,
  changing channels or using menus; its non-battery icons no longer disappear
  for about one second and reappear.
- Added normal keypad-lock behaviour to both FM overlays. Holding F toggles the
  resident lock, locked keys are ignored, and the resident F/lock indicators
  remain synchronized rather than being redrawn privately by the app.
- Reduced FM VFO's overlay-RAM state by compiling Memory-only name-editor data
  only into FM Memory. Both applications still preserve all existing FM
  settings, names and 48 saved stations.
- **Reinstall both `FMVFO.app` and `FMMemory.app`** to receive these changes.
  Installing only one leaves the paired FM interface at mixed versions.

### Messenger, HEARD and status/audio feedback

- When AUTO CHECK cannot transmit because of No FSK, frequency lock, modulation,
  configuration or battery policy, it now behaves like manual SEND PING: AUTO
  CHECK is cancelled and the same visible TX-blocked notice is shown. A blocked
  background attempt brings HEARD forward so the reason cannot expire unseen.
- The TX-blocked warning now plays immediately rather than being queued until a
  later key press. Its new restrained negative alert is a 650 Hz short/long
  `dit, diit`, clearly different from PONG/ACK and new-message sounds.
- Replaced the Power Save text with the requested compact 9x7 `Zz` symbol and
  refreshed the 11x7 unread-message envelope, 10x7 AUTO CHECK chain link, No
  FSK and No Roger marks from the supplied pixel references. Status-bar symbols
  are top-aligned with the existing resident icons.
- Kept AUTO CHECK and unread-message state independent: the chain link still
  reports the most recent automatic PING result while the envelope is driven
  only by unread Inbox messages.

### Applications and reliability

- Updated the APRS DIGI source to **0.2-gogu**. It now reads the shared APRS
  profile/callsign configured by APRS Edit through the bounded config proxy and
  no longer stores a conflicting private one-byte mode. APRS DIGI is not one of
  the three overlay binaries bundled with this release.
- Updated the CHIRP module's driver, variant and firmware-comparison identifiers
  to 3.6.0. Its channel/settings schema is unchanged from the 3.3.5 module.
- Saved 520 bytes of resident RAM without reducing the 4 KiB runtime stack or
  4 KiB overlay workspace: UART and USB share one synchronous decoded-command
  staging buffer (256 bytes), the full-speed USB endpoint buffer is one packet
  instead of two (64 bytes saved), and RF Log Lite's guarded 200-byte entry
  array occupies the otherwise unused gap before the fixed overlay anchor.
  UART and USB RX rings remain independent.
- Preserved RF Log Lite's 20-entry behaviour, CHIRP/K5Viewer USB operation,
  multiboot workspace isolation and all normal radio functions. Host regression
  tests pass under AddressSanitizer and UndefinedBehaviorSanitizer for the font
  dictionary/framebuffer, USB, Messenger protocol/store and HEARD layout.

### Compatibility, release files and build information

- `Search.app` remains **1.5-gogu** and is byte-for-byte identical to the 3.3.5
  release (`SHA-256 acda9da...5753`). It is supplied as the requested Scan/Search
  package for convenience and does not need to be reinstalled if already present.
- `FMVFO.app` and `FMMemory.app` changed and should be reinstalled together.
- Old custom GOGUFW binaries made for the pre-3.3.1 experimental API 4–9 layout
  remain incompatible. Official API-3 apps made for GOGUFW 3.3.1 and later are
  still compatible because official API 4 only appends services.
- Firmware: `GOGUFW-v3.6.0.bin`
- Multiboot firmware: `GOGUFW-v3.6.0-multiboot.bin`
- CHIRP module: `Gogufw_3.6.0_chirp_module.py`
- Overlay packages: `Search.zip`, `FMVFO.zip`, `FMMemory.zip` and the combined
  `Apps.zip` containing those three applications.
- Final published image: **120,028 / 120,832 bytes** (804 bytes free), including
  the 920-byte multiboot load image. Linked code/data uses 119,108 bytes.
- RAM with the enforced stack reservation: **15,840 / 16,384 bytes** (544 bytes
  free); the 4,096-byte runtime stack and multiboot RAM-stub isolation checks pass.

## GOGUFW 3.3.5 — 2026-11-01

- Replaced the generic new-message double beep with the measured timing of the
  requested classic Nokia alert: four equal 932 Hz, 200 ms notes in two close
  pairs, with an approximately 735 ms pause between the pairs. ACK packets no
  longer enter the new-message alert path and now use the same short 500 Hz
  confirmation chirp as a manual PONG. Both remain controlled by `MsgBeep`.
- Extended HEARD with a MENU containing manual **SEND PING** and background
  **AUTO CHECK** choices for OFF, 30 seconds, 1 minute, 5 minutes and 10 minutes.
  Automatic replies update HEARD silently; each unanswered check produces one
  restrained 450-to-320 Hz descending alert directly from the background tick
  instead of accumulating until a key is pressed. This is distinct from the
  positive manual-PONG chirp and the new-message alert. Checks continue
  on every normal firmware screen and defer only while voice TX/RX or scanning
  owns the RF path.
- Added a chain-link AUTO CHECK result indicator using the otherwise
  empty VOX slot. Selecting an interval sends the first PING immediately and
  leaves the result slot blank until it completes: the link stays fixed after
  a PONG and blinks at one-second intervals after an unanswered 12-second
  window. Later checks retain the previous completed result until the new
  result is known. The real VOX icon has priority whenever VOX is enabled, and
  the Messenger envelope, F/key-lock, mute/backlight and battery areas remain
  unchanged.
- Decoupled the Messenger envelope from AUTO CHECK. It now follows only the
  Inbox unread flags and refreshes immediately when the final unread message is
  opened or deleted.
- Made private overlay tuning explicitly end resident RX state and pending
  dual-watch activity before taking ownership of the radio, preventing a late
  resident squelch event from pulling an active overlay back to the main screen.
- Recalibrate the VFO Spectrum scale and trigger after `1`/`7` scan-step
  changes. This prevents ordinary traces from falling below the graph when the
  BK4819 scan filter switches between the 15 kHz-and-up and sub-15 kHz paths.
- Apply the same recalibration after `0` switches between FM, AM and USB, so a
  previous modulation's noise floor cannot hide the new trace.
- There is no overlay ABI/API change in 3.3.5. Applications built for the
  official F4HWN 6.3.0 ABI 1 / API 3 contract used by GOGUFW 3.3.1 and 3.3.2
  remain compatible; a firmware-only update is supported.
- `FMVFO.app`, `FMMemory.app` and `Search.app` are included again for
  convenience but are unchanged from 3.3.2. If they are already installed,
  they do not need to be reinstalled; updating only the firmware is sufficient.
- Firmware: `GOGUFW-v3.3.5.bin`
- Multiboot firmware: `GOGUFW-v3.3.5-multiboot.bin`
- CHIRP module: `Gogufw_3.3.5_chirp_module.py`
- Final published image: 118,992 / 120,832 bytes (1,840 bytes free), including
  the 920-byte multiboot load image. Linked firmware code/data uses 118,072
  bytes; RAM usage is 16,360 / 16,384 bytes, and multiboot RAM-stub isolation
  checks pass.

## GOGUFW 3.3.2 — 2026-10-26

- Ported the insertion-style text editor from Messenger PR #14 to every
  resident text-entry screen: Messenger Compose, channel name, Messenger
  callsign and FM channel name. UP/DOWN (LEFT/RIGHT on UV-K1) moves a visible
  caret, insertion and backspace work in the middle of text, and `0` inserts a
  space immediately. Input modes now display as `ABC`, `abc` and `123`. The
  caret stays under a multi-tap character until it is committed. Thanks to
  [@booqoffsky](https://github.com/booqoffsky) for proposing and implementing
  the original interaction improvements in
  [PR #14](https://github.com/Gogu-Qs/GOGUFW-UV-K1-Messenger/pull/14).
- Applied immediate space insertion to APRS Edit and moved FM Memory name entry
  onto the same resident editor through the append-only GOGU extension level 2.
  The new FM Memory declares its capability so older firmware rejects it
  cleanly instead of attempting to call an unavailable service.
- Preserved the navigation-layout setting across power cycles in non-RescueOps
  builds and made Reset All clear both the navigation and key-lock bits.
- Fixed the Spectrum graph in both VFO and Memory modes so weak traces remain
  visible above the bottom axis. Initial scaling now also considers the weakest
  sample in the completed sweep, making the scale easier to recover on mixed
  or quiet channel ranges.
- Renamed the assignable side-key action from `PTT` to `PTT MODE` in the
  firmware and CHIRP module. Its behavior is unchanged: it switches the SetPTT
  setting between Classic and OnePush; it is not a transmit command.
- There is no breaking overlay ABI/API compatibility change in this release. Existing
  GOGUFW 3.3.1 applications remain compatible and do not need to be reinstalled;
  users may update only the firmware. Refreshed FM Memory/APRS Edit builds are
  optional and only carry the new text-entry behavior.
- Firmware: `GOGUFW-v3.3.2.bin`
- Multiboot firmware: `GOGUFW-v3.3.2-multiboot.bin`
- Optional refreshed overlays: `FMMemory.app`, `APRSEdit.app`
- CHIRP module: `Gogufw_3.3.2_chirp_module.py`
- Final Flash image: 117,864 / 120,832 bytes (2,968 bytes free), including the
  920-byte multiboot RAM-stub load image; isolation checks pass.

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
