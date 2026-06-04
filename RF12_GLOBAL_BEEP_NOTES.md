# GGFW RF12 - Global Messenger Notification Beep

Base: RF11 boot-prime + REG_3F OR-lock.

Goal:
- Preserve RF11 message reception from boot.
- Preserve normal voice RX/TX path.
- Redesign real RF message notification beep so it is not tied to Messenger UI/Inbox screens.

Changes:
- Real RF message decode now sets a global deferred beep flag.
- `MSG_RF_Tick10ms()` plays the beep directly with `AUDIO_PlayBeep(...)` after RX/FSK activity is idle.
- The old UI-local `gBeepToPlay` request is cleared for real RF messages to avoid delayed beep only when entering Inbox.
- Demo/test messages still use the existing store-layer beep behavior.

Test:
1. Boot both radios, do not open Messenger on receiver.
2. Send messages from the other UV-K1 while receiver is on main screen.
3. Expected: envelope/inbox update + audible bipbip after RX goes idle.
4. Test again while receiver is inside Messenger HOME, then while entering Inbox.
5. Expected: no duplicate delayed Inbox-only beep.

RF safety:
- No change to REG_47 handling.
- No change to RF11 boot-prime / REG_3F OR-lock logic.
- No new RX-start register writes.
