# GOGUFW 2.0.1

GOGUFW 2.0.1 is a focused Messenger and Range Check safety/reliability update for the Quansheng UV-K1 / UV-K5 V3. Packet framing and compatibility with existing GOGUFW radios are unchanged.

## Scan, Dual Watch and reply-channel fixes

- When a valid FSK message or PING is received during scanning, normal Scan progression and Dual Watch switching pause until the pending ACK/PONG is sent, cancelled or expires.
- Delayed ACK/PONG entries retain the receive-side VFO, RX frequency and actual TX frequency, including the configured repeater offset.
- Automatic replies temporarily disable cross-band selection while targeting the captured receive VFO, then restore the user's original cross-band setting.
- If the channel or its RX/TX frequency changes before a queued response is sent, the response is cancelled and scanning is released instead of transmitting on another channel.
- Text retries retain the original TX VFO and frequencies. A changed or disallowed target cancels the retry.

## FSK transmission safety

- The configured **F Lock plan is now a hard boundary** for Messenger and Range Check FSK transmission in both VFO and memory mode.
- The actual TX frequency is checked before wake, text, retry, PING, ACK and PONG transmission, including memory-channel TX offsets.
- Outside-plan transmission is blocked even if the memory channel's separate TXLock override is disabled. This deliberately prevents unattended FSK replies from bypassing the configured frequency plan.
- FSK TX is also rejected in AM/non-FM modes, during serial configuration and at invalid battery levels.
- A manually blocked message, resend or PING keeps the current screen open and displays a two-second floating **TX BLOCKED / SEND CANCELLED** notice with four short error beeps.
- A blocked message is not added to Sent and a blocked PING does not start the Range Check result timer. Automatic ACK/PONG rejection remains silent and releases the transaction normally.

## Multi-radio Range Check

- PONG scheduling now mixes the responder's MsgCsg/callsign, PING ID, received RSSI and runtime entropy.
- Responders select one of six 1.2-second slots beginning 3–9 seconds after PING reception, reducing simultaneous PONG overlap when several radios respond.
- Existing carrier checks remain enabled; a busy channel defers the automatic response.
- The packet format and complete 12-second result collection window are unchanged.
- Slot selection reduces collisions but does not guarantee collision-free reception when multiple devices choose the same slot.

## Text-entry fix

- ChName and MsgCsg now use the same press/release handling as Messenger Compose.
- A long numeric-key press enters only the digit instead of leaving an extra provisional letter such as `d3` or `j5`.
- Short-press multi-tap entry, timeout behavior, deletion and existing Compose/FM naming behavior remain unchanged.

## Version and build

- Welcome/version strings, UART identity and new multiboot slot metadata report **v2.0.1**.
- Clean ARM GNU Fusion build and multiboot RAM-stub isolation check passed.
- FLASH: **118,184 / 120,832 bytes (97.81%)**
- RAM: **13,488 / 16,384 bytes (82.32%)**

## Downloads

- `f4hwn.gogufw.v2.0.1.bin` — canonical multiboot-compatible firmware image
- `Gogufw_2.0.1_chirp_module.py` — matching CHIRP module under Quansheng

## Thanks

Special thanks to [@mkalin22](https://github.com/mkalin22) for detailed real-radio testing and reports, especially the Scan/PONG channel-context and TX-safety findings that led to these fixes.

## Hardware warning

This firmware is only for Quansheng **UV-K1 / UV-K5 V3** radios using the **PY32F071 MCU and BK4829 RF IC**. Do not install it on UV-K5 V1/V2 or other BK4819-based models.

Back up the radio with CHIRP before updating. Keep a known-good DFU recovery image available when using multiboot.
