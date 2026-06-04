# GOGUFW 0.5.18 Release Notes

Public source update from the older 0.3.x GitHub state to the current 0.5.18 line.

## Main user-visible changes since 0.3.12

### Messenger

- Boot-time Messenger receive support.
- ACK/retry system with text ACK fallback.
- VFO-aware ACK handling: ACK is sent back on the VFO/channel where the message was received, then the previous VFO state is restored.
- Inbox, Sent and Drafts UI improvements, including 6-row lists and read-screen navigation.
- Sent RESEND support.
- Shared 36-character message limit across Compose, Reply, Drafts and TX builder.
- Persistent Drafts and Messenger settings.
- T9 editor improvements:
  - 800 ms multi-tap timeout.
  - Uppercase/lowercase letter modes cycle only letters.
  - Numeric mode remains digit-only.
- Duplicate-message protection groundwork: retry duplicates should not create extra inbox entries.

### Range Check

- UV-K1 to UV-K1 Range Check based on the existing Messenger FSK infrastructure.
- Short ping/pong sound feedback.
- Result screen with ID, RSSI, voltage and signal bars.
- Cold-boot Range Check initialization improvements.
- 0.5.18 timing refinement:
  - PING repeats remain unchanged.
  - PONG repeats remain unchanged.
  - PONG response timing is shifted later to reduce overlap with repeated PING bursts while staying inside the 10 second listen window.

### CALLTX / Call Tone

- F+9 assigned to CALLTX.
- Selectable call tone menu.
- Separate PREVIEW screen for tone selection.
- Preview playback avoids automatic tone playback while browsing.

### Backlight shortcut hotfix

F+9 remains assigned to CALLTX. To keep the original backlight strategy accessible, F+8 now cycles through three states:

1. Backlight Always ON
2. Backlight Always OFF
3. Return to normal BackLt strategy

This replaces the old dependency on F+9 for returning from manual backlight management.

### SysInfo

- Build commit fallback changed from `unknown` to `source-zip` when building from a ZIP without `.git` metadata.
- SysInfo QR rebrand work from the earlier 0.3.x line is preserved.

### Storage safety

- Messenger config is stored in the GOGUFW private flash sector at `0x012000`.
- The old 0.3.3 EEPROM compatibility address `0x1E80` is not used because it overlaps channel-memory compatibility data.

## Notes for testers

- Messenger remains the primary feature and should be tested after any Range Check change.
- Current Range Check timing intentionally keeps the listen window at 10 seconds.
- If Range Check fails after long idle time while Messenger continues to receive normally, the likely problem is Range Check state/re-arm logic rather than generic FSK RX failure.
