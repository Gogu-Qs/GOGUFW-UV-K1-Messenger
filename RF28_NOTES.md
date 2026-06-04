# RF28 - Event re-prime + ACK hidden + read counter

Base: RF27/RF26 line.

Changes:
- Disabled periodic 8-second idle re-prime. Re-prime now runs only when explicitly scheduled after TX/RX-like events.
- Kept TX/RX event re-prime and ACK timing from RF27.
- Text ACK (`ACK:hhhh`) is no longer stored in Inbox; it only updates Sent ACK status.
- Inbox/Sent read screen shows current position at top-right (`1/5`, `2/5`, etc.).
- RF26 numeric T9 and RF21 UI/branding are preserved.

Test focus:
- General message receive ratio should recover from RF27 regression.
- ACK should still be possible after TX-side event re-prime.
- ACK text should not appear as an Inbox message.
- Read screens should show `n/total` and UP/DOWN navigation should still move between messages.
