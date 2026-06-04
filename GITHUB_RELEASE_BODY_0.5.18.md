# GOGUFW 0.5.18

This release updates the public GitHub source from the older 0.3.x line to the current 0.5.18 development line.

## Highlights

- Messenger boot-time receive, ACK/retry, text ACK fallback and VFO-aware ACK handling.
- Improved Inbox/Sent/Drafts UI, Sent RESEND, Reply/Delete flows and persistent drafts/settings.
- T9 editor improvements: 800 ms multi-tap timeout and letter-only ABC/abc cycling.
- UV-K1 ↔ UV-K1 Range Check using the Messenger FSK infrastructure.
- Range PONG timing refined to avoid early overlap with repeated PING bursts.
- F+9 CALLTX with selectable call tone preview screen.
- F+8 backlight shortcut now cycles: Always ON → Always OFF → normal BackLt strategy.
- SysInfo ZIP-build fallback changed from `unknown` to `source-zip`.

## Important note

Messenger config uses the private flash sector at `0x012000`. The old 0.3.3 `0x1E80` storage location is not used because it overlaps channel-memory compatibility data.

## Build

```bash
chmod +x ./compile-with-docker.sh
./compile-with-docker.sh
```

The internal build preset is still named `Fusion`, but the visible firmware branding is `GOGUFW 0.5.18 / GGFW`.
