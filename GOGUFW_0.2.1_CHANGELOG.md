# GOGUFW 0.2.1 Changelog

- Unread message LED repeat interval changed from 10s to 3s while keeping the same double-blink pattern.
- MSGDBG moved out of the public main menu into the hidden boot menu area.
- Compose/Reply/Draft editor: long-press 0-9 inserts that digit while staying in B/b text mode.
- Read screen UI refined: compact one-line metadata, lighter dotted separators, footer actions REPLY/RESEND and F:DEL.
- Compose/Draft footer refined: SEND on the left, B/b/2 mode indicator on the right.
- ACK sending now uses short random jitter to reduce multi-radio ACK collisions.

Deferred:
- FM broadcast memory naming remains planned for a later 0.3.x feature release.
- Dual-watch AM/FM FSK restore investigation remains deferred to a separate RF-safe test branch.
