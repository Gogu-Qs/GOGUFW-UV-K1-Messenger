# GOGUFW 0.5.18

GOGUFW is a messaging-focused F4HWN Fusion-based firmware fork for Quansheng UV-K1 / BK4829 radios.

This package updates the public GitHub source from the older 0.3.x line to the current 0.5.18 development line.

## Main features

- UV-K1 Messenger with Inbox, Compose, Sent, Drafts, Reply, Delete and Resend flows
- Boot-time RF message receive without opening Messenger first
- ACK/retry messaging with text ACK fallback and VFO-aware ACK return
- Persistent Messenger settings, drafts and callsign storage using the GOGUFW private flash sector
- Shared T9 editor with multi-tap timeout and letter-only ABC/abc cycling
- Global message beep, unread icon and unread notification behavior
- Range Check for UV-K1 to UV-K1 testing using the Messenger FSK infrastructure
- F+9 CALLTX call-tone feature with selectable tone preview screen
- FM broadcast radio UI refinements and memory-channel naming groundwork
- VS Code and Docker build support

## 0.5.18 highlights

- Range PONG timing refined to avoid early overlap with repeated PING bursts.
- Range PING repeat, PONG repeat and Messenger ACK repeat behavior preserved from the previous stability line.
- F+8 backlight shortcut changed to a 3-step cycle:
  1. Backlight Always ON
  2. Backlight Always OFF
  3. Return to normal BackLt strategy
- F+9 remains assigned to CALLTX.
- SysInfo build commit fallback changed from `unknown` to `source-zip` when the source tree is not a git checkout.

## Important storage note

GOGUFW Messenger configuration uses the private flash sector at `0x012000`.

The older 0.3.3 storage location at EEPROM compatibility address `0x1E80` was intentionally abandoned because it overlaps the channel-memory compatibility area. No automatic migration is performed from that legacy address for safety.

## Build

Docker Desktop must be installed and running.

```bash
chmod +x ./compile-with-docker.sh
./compile-with-docker.sh
```

The internal CMake preset is still named `Fusion` because GOGUFW uses the F4HWN Fusion feature set. The visible firmware branding/version is `GOGUFW 0.5.18 / GGFW`.

## VS Code build

See `BUILD_WITH_VSCODE.md`.

## Changelog / release notes

- Current release notes: `GOGUFW_0.5.18_RELEASE_NOTES.md`
- Range timing notes: `GOGUFW_0.5.18_RANGE_PONG_TIMING_NOTES.md`
- Previous hotfix notes are kept in the repository for development history.

## Attribution

Based on the F4HWN / UV-K5 custom firmware project. Original license and attribution are preserved in this repository.
