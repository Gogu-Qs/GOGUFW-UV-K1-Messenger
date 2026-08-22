# GOGUFW Development Instructions

## Project Context

This repository contains the GOGUFW firmware for Quansheng UV-K1 / UV-K5 V3 hardware based on the PY32F071 MCU and BK4829 RF IC.

The firmware is derived from Armel/F4HWN and includes a custom Messenger subsystem, HEARD screen, Range Check, Call Tone, FM station naming, FM RSSI display, and related UI and RF extensions.

The primary goal is to preserve the reliability of the Messenger system while selectively integrating safe improvements from upstream F4HWN.

Do not treat this repository as a clean upstream F4HWN tree.

---

# Primary Rule

**Messenger reliability has priority over all other features.**

Any change that may affect:

- RF initialization
- RX state
- squelch
- audio path
- BK4829 registers
- IRQ masks
- power save
- dual watch
- scanning
- TX → RX transition
- FSK reception
- EEPROM / external flash layout

must be reviewed for possible Messenger regressions.

Do not make large speculative RF changes.

Prefer minimal and reversible changes.

---

# Current Known-Good Messenger Design

Preserve the following behavior unless explicitly instructed otherwise.

## FSK RX

FSK RX is implemented as a sidecar to normal analog radio operation.

Do not redesign the radio around Messenger.

The normal F4HWN analog RX state machine should remain authoritative wherever possible.

### FSK IRQ rules

- Arm the FSK interrupt mask only when FSK RX is actually armed.
- Do not periodically rewrite the IRQ mask every 10 ms.
- Do not add periodic aggressive BK4829 re-prime logic.
- Do not add continuous keepalive reinitialization.
- Avoid hard radio restores unless there is a proven reason.

### TX → RX behavior

After Messenger TX:

- return to normal RX using the existing delayed re-arm behavior
- do not immediately perform aggressive RX reset sequences
- preserve analog squelch and speaker behavior

---

# Messenger Wake Preamble

The first transmission attempt of a normal Messenger message sends a short invisible FSK wake preamble before the real packet.

Rules:

- Wake preamble is sent only before the first message transmission.
- Retries send only the real message.
- Do not send the wake preamble before every retry.
- Do not change this behavior without explicit approval.

Range Check has its own transmission behavior and must not automatically inherit normal Messenger wake logic.

---

# Messenger Packet Compatibility

Packet compatibility is a hard requirement.

Do not change:

- packet framing
- message IDs
- callsign handling
- ACK packet format
- retry behavior
- ping / pong format
- duplicate detection behavior

unless explicitly requested.

Existing GOGUFW radios must remain mutually compatible.

---

# ACK / Retry Behavior

Preserve existing ACK and retry behavior.

ACK reception must work independently of the currently displayed screen.

ACK processing must continue while the user is on:

- main radio screen
- Messenger
- Sent
- Inbox
- Drafts
- HEARD
- Range Check

Do not make ACK processing dependent on UI state.

---

# Range Check

Range Check uses the Messenger/FSK RF subsystem but is logically separate.

Preserve:

- ping transmission
- pong response
- ACK / result collection
- callsign / device identification
- RSSI and voltage result reporting

Range Check must not break Messenger after use.

Messenger must not permanently break Range Check after use.

Any state reset added to Range Check must be isolated and reversible.

---

# HEARD

HEARD tracks recently received Messenger-related packets.

Current intended capacity is small by design to avoid RAM corruption.

Do not significantly increase HEARD storage without checking RAM usage.

HEARD RSSI display uses the five-bar UV-K5-style baseline geometry.

Keep HEARD and Range Check RSSI bar appearance consistent.

---

# VOX

When VOX is enabled:

- normal microphone/audio TX must work
- Messenger FSK sidecar RX is disabled as required by the current design
- normal voice audio path must be restored

When VOX is disabled:

- Messenger FSK RX should become available again

Do not reintroduce earlier VOX behavior where TX occurred without microphone audio.

---

# Analog RX / Squelch

Normal F4HWN analog RX behavior should remain authoritative.

Messenger must not permanently override:

- `gEnableSpeaker`
- squelch-open state
- squelch-close state
- normal audio routing

FSK soft-mute may temporarily modify BK4829 audio gain/register state when necessary, but it should avoid restoring stale high-level radio state.

Never restore a previously saved `gEnableSpeaker = true` value after the stock squelch logic has already closed the speaker.

When investigating squelch problems, compare behavior against the matching upstream F4HWN implementation before inventing new state machines.

---

# FM Radio

The firmware includes:

- FM station memory names
- save / delete / rename behavior
- FM RSSI display
- continuous RSSI updates

Known concern:

FM RSSI polling/display updates must not cause audible periodic clicking.

Avoid unnecessary LCD redraws or frequent BK1080 register activity.

Prefer cached RSSI values and redraw only when the visible RSSI level changes.

---

# CHIRP

The custom CHIRP module must stay synchronized with firmware enums.

Important:

Firmware key-action options currently include custom GOGUFW actions such as:

- MESSENGER
- HEARD
- CALLTX

The CHIRP key-action list must use exactly the same numeric ordering as the firmware enum.

If CHIRP fails to recognize a key-action value, it may convert it to `None` and overwrite the user's radio setting on the next upload.

Always compare CHIRP enum ordering against the firmware definition before adding or removing actions.

---

# Shortcut Behavior

Messenger and HEARD shortcuts behave as toggles.

Expected behavior:

- first shortcut press opens the screen
- pressing the same shortcut while already on that screen closes it

CALLTX behavior is separate and should not automatically be changed to toggle behavior.

---

# CALLTX

CALLTX uses the existing call-tone implementation.

Do not reintroduce abandoned call-volume experiments.

The stable implementation uses selectable call tones and the existing radio TX path.

---

# Power Save / Screen Off

Historically, power-save and screen-off logic has interfered with Messenger reception.

Treat any changes to:

- sleep
- BK4829 power state
- display-off mode
- wake windows
- dual watch timing

as high-risk.

Messenger RX must continue to work when possible without requiring the user to manually enter the Messenger screen.

Incoming FSK handling should remain screen-independent.

---

# Upstream F4HWN Integration

Do not merge upstream versions wholesale.

For each upstream change:

1. Identify the files/functions affected.
2. Determine whether the code intersects with Messenger, FSK, RF, power-save, dual-watch, scan, EEPROM, or UI.
3. Classify the change as:
   - safe
   - useful but needs adaptation
   - high-risk
   - unnecessary
4. Apply only the required change.
5. Build and regression-test afterward.

Prefer upstream bug fixes when they solve a problem cleanly.

Avoid importing large new subsystems merely because they exist upstream.

---

# Current Upstream Target

The current comparison target is F4HWN 5.9.0.

The current strategy is:

- fix GOGUFW bugs
- selectively integrate safe improvements through F4HWN 5.9.0
- postpone 6.0.0 / multiboot integration until the official implementation is available

Do not attempt to invent a custom multiboot system unless explicitly requested.

---

# Multiboot / External SPI Flash

Future multiboot work is high-risk.

Before allocating any external flash space, derive the real memory map from source.

Verify all existing consumers including:

- channel memory
- radio settings
- FM station names
- Messenger persistent storage
- Range Check state
- calibration
- scan lists
- profiles
- logs
- bootloader state
- firmware slots

Never guess offsets.

Never overwrite calibration or radio state areas.

---

# Memory Constraints

This firmware runs close to MCU resource limits.

After meaningful changes, check:

- FLASH usage
- RAM usage
- stack risk
- static buffers

Avoid large static arrays.

Avoid large debug buffers.

Avoid adding persistent debug systems unless explicitly requested.

Do not enable old Messenger debug counters, MsgDbg screens, demo injection, or test packet generators by default.

---

# Build Rules

Primary build environment:

- macOS
- Homebrew
- `arm-none-eabi-gcc`
- CMake
- Ninja
- VS Code

Preferred build:

```bash
cmake --preset Fusion
cmake --build --preset Fusion
```

Do not configure the firmware using the macOS host Clang toolchain.

The ARM toolchain must be active.

Before delivering a source package:

- remove stale `build/`
- remove `CMakeCache.txt`
- remove machine-specific absolute build paths
- ensure VS Code tasks work from a freshly extracted directory

---

# Build Validation

After modifying firmware:

1. Perform a clean configure.
2. Perform a clean build.
3. Confirm there are no compiler errors.
4. Record FLASH usage.
5. Record RAM usage.
6. Verify firmware/version identity when relevant.

Do not call a change complete if it has not been built.

---

# Regression Test Checklist

For RF-related changes, verify at minimum:

- normal analog RX
- squelch closes correctly after carrier loss
- PTT TX audio
- VOX TX audio
- Messenger TX
- Messenger RX
- first-message wake preamble
- ACK reception
- ACK retry
- Inbox
- Sent
- Drafts
- reply
- resend
- HEARD
- Range Check ping
- Range Check pong
- Range Check result reception
- CALLTX
- FM radio audio
- FM RSSI
- Dual Watch
- power-save / screen-off behavior where applicable

Pay particular attention to Messenger reception after:

- normal voice RX
- voice TX
- FM radio use
- Range Check use
- scanning
- screen sleep
- long idle periods

---

# Debugging Philosophy

Prefer evidence over speculation.

When a problem appears:

1. reproduce it
2. locate the exact state transition
3. compare the corresponding code with upstream F4HWN
4. identify the smallest divergent behavior
5. patch only that behavior
6. rebuild
7. regression-test Messenger

Avoid repeated trial-and-error register writes.

Avoid aggressive full-radio resets as general-purpose fixes.

---

# Git / Change Discipline

Keep changes focused.

For non-trivial work:

- inspect `git diff` before editing
- make one logical change at a time
- avoid unrelated formatting changes
- keep patches easy to review and revert

Before finishing:

- show the relevant diff
- summarize why each change was necessary
- mention possible RF/Messenger regression points
- provide build FLASH/RAM results

Do not silently rewrite unrelated GOGUFW behavior.

---

# Important Existing Design Decisions

The following decisions are intentional and should not be "cleaned up" without explicit approval:

- Messenger is the primary GOGUFW feature.
- No 10 ms FSK IRQ refresh.
- No startup FSK keepalive.
- No aggressive continuous FSK re-prime.
- Delayed FSK RX re-arm after TX.
- Wake preamble only before first message attempt.
- Retries do not send the wake preamble.
- ACK handling is independent of UI screen.
- Range Check must coexist with Messenger.
- VOX disables Messenger FSK sidecar while active.
- FSK should not own analog speaker/squelch state.
- MsgCtx menu option was removed.
- MsgDbg/debug counters are not part of the production firmware.
- Messenger and HEARD shortcuts toggle their screens.
- CALLTX behavior remains separate.

---

# Communication With User

Before making a large or risky RF change:

- explain what subsystem will be modified
- identify likely regression risks
- prefer a minimal patch
- do not silently redesign existing behavior

When the user reports test results, treat real-radio observations as higher priority than theoretical assumptions.

If multiple approaches are possible, prefer the one that preserves currently working Messenger behavior.