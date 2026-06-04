# GOGUFW 0.2.1 public cleanup build

## Public-release changes

- Version/branding updated to GOGUFW 0.2.1 / GGFW on visible firmware strings.
- ACK ON/OFF fixed as a safety-critical setting:
  - ACK OFF disables automatic ACK TX.
  - ACK OFF disables ACK wait/retry state for manual sends.
  - Incoming messages still enter Inbox and can still trigger beep/LED.
- MSGHOP hidden from the public radio menu until hop/relay is implemented.
- Messenger HOME simplified:
  - Inbox
  - Compose
  - Sent
  - Drafts
- Messenger-local Settings row removed because settings live in the main radio menu.
- Inbox/Sent read pages now frame message text between two horizontal lines.
- FSK audio mute added:
  - FSK burst mutes speaker/audio path.
  - Audio is restored on successful decode, failed decode timeout, PTT/TX, or RF restore.
- Unread message LED notification added:
  - Works whenever unread messages exist, not only when screen is off.
  - Approx. two short flashes every 10 seconds.
  - Obeys the main menu LED notification setting.
  - Does not intentionally override normal RX green / TX red LED behavior.
- Build helper simplified:
  - Public build command is now `./compile-with-docker.sh`.
  - Only the GGFW/Fusion-based build target is exposed.
