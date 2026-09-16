# GOGUFW 2.0.2

GOGUFW 2.0.2 adds per-memory-channel control over Messenger/Range Check FSK transmission and Roger/MDC tails, together with matching CHIRP support and main-screen indicators. It also refines CALLTX melodies and FM broadcast station-name editing. Messenger packet framing and compatibility with existing GOGUFW radios are unchanged.

## Per-channel FSK transmission policy

- Added a **No FSK TX** property to every memory channel.
- When enabled, all outgoing Messenger/Range Check FSK is blocked on that channel: wake frames, text messages, retries, PING, ACK and PONG.
- The check runs before the FSK warm-up/wake path, preventing even a preliminary FSK transmission.
- Normal analog voice TX and incoming FSK reception remain enabled; this is not a general channel TX lock.
- Manual message, resend and PING attempts use the existing two-second **TX BLOCKED / SEND CANCELLED** notice and four forced warning beeps.
- Automatic ACK/PONG responses are cancelled silently so unattended reception does not generate pop-ups or warning tones.
- The existing F Lock, TXLock, FM-only modulation, battery and serial-configuration safeguards remain active in addition to this channel policy.

This option is useful for amateur repeaters, shared channels and other memories where receiving Messenger packets is acceptable but accidental FSK transmission is not.

## Per-channel Roger/MDC policy

- Added a **No Roger** property to every memory channel.
- When enabled, it suppresses the selected Roger beep or MDC burst after an ordinary voice transmission on that channel, even if Roger is enabled in the global radio menu.
- DTMF end-of-transmission data, CTCSS/DCS tail handling and the normal TX-to-RX restoration path are unchanged.
- CALLTX now always skips the normal Roger/MDC tail because the transmission already consists of an alert melody. Ordinary PTT/VOX voice transmission still follows the global Roger setting and the channel's `No Roger` override.

## Main-screen channel symbols

The former selected-channel-only SQL label area now shows compact capability symbols for each displayed memory channel:

| Symbol | Meaning |
| --- | --- |
| Radio-wave/RSS-style icon | FSK transmission is permitted on this memory channel (`No FSK TX` is off). |
| Musical-note icon | Roger/MDC is globally enabled and permitted on this memory channel (`No Roger` is off). |

- Both upper and lower memory-channel rows show their own state.
- The icons remain hidden in VFO mode because these are stored-channel policies.
- An absent radio-wave icon means FSK TX is disabled for the channel.
- An absent note can mean either that Roger is globally off or that `No Roger` is enabled for the channel.
- Icon spacing is compatible with both Normal and Tiny GUI layouts, including WIDE bandwidth labels.

## CHIRP configuration

Use the matching `Gogufw_2.0.2_chirp_module.py`; older modules do not understand the new packed channel flags.

1. Start CHIRP and choose **File → Load Module**.
2. Select `Gogufw_2.0.2_chirp_module.py`, then download the radio using the GOGUFW model under **Quansheng**.
3. For one channel, edit **No FSK TX** and **No Roger** directly in their normal memory-list columns.
4. For a bulk change, select multiple channel rows, right-click and open **Properties/Edit**, then open the **Extra** tab.
5. Select **No FSK TX** and/or **No Roger** for the selected channels as required.
6. Upload the completed channel image to the radio.

These settings are available both as normal memory-grid columns and in **Properties → Extra** for bulk operations. The module reads and writes them together with the channel step byte while retaining compatibility with existing 2.0.1 channel images and erased/default records.

## CALLTX melody fixes

- The three-second CALLTX duration is now a minimum boundary rather than a hard mid-note cutoff.
- Playback finishes the current complete melodic phrase before ending TX, so the last note is no longer lost.
- Configured gaps between notes now disable tone modulation for real silence while retaining the RF carrier.
- All five call melodies use the corrected playback path.

## FM broadcast name editor

- Holding a numeric key while editing an FM station name now inserts the digit as soon as the long-press threshold is reached.
- The user no longer has to release the key before the digit appears.
- Releasing the key after the long press does not insert a duplicate digit.
- This now matches ChName and MsgCsg text-entry behavior.

## Spectrum display

- The manual RSSI/trigger label now uses the compact form `M-110/-120` instead of `M -110/-120`.
- This restores separation between the threshold values and long memory-channel names.

## Version and build

- Firmware identity, CMake preset and GitHub Actions artifact name report **v2.0.2**.
- Clean ARM GNU Fusion build and multiboot RAM-stub isolation check passed.
- FLASH: **118,288 / 120,832 bytes (97.89%)**
- RAM: **13,488 / 16,384 bytes (82.32%)**

## Downloads

- `f4hwn.gogufw.v2.0.2.bin` — canonical multiboot-compatible firmware image
- `Gogufw_2.0.2_chirp_module.py` — matching CHIRP module under Quansheng

## Thanks

Special thanks to [@mkalin22](https://github.com/mkalin22) for detailed real-radio testing and reports that continue to improve Messenger, Range Check and transmission safety.

## Hardware warning

This firmware is only for Quansheng **UV-K1 / UV-K5 V3** radios using the **PY32F071 MCU and BK4829 RF IC**. Do not install it on UV-K5 V1/V2 or other BK4819-based models.

Back up the radio with CHIRP before updating. Keep a known-good DFU recovery image available when using multiboot.
