# GOGUFW 0.3.3 Changelog

- Fixed blocker menu value corruption where MSGCSG text could appear in original F4HWN menu values such as SETTMR, STE, D ST, D LIVE, BUSYCL, SCPRI and BEEP.
- Restored original boolean menu value rendering by moving MSGCSG rendering out of the shared OFF/ON case group.
- Kept MSGCSG function intact and still persistent.
- Centered Messenger bold titles using screen-width based positioning adjusted for the SmallBold font pitch.


## 0.3.2 hotfix
- Fixed ACK wait timer overflow by changing `s_wait_ack_ticks` from `uint8_t` to `uint16_t`.
- `MSG_RF_ACK_TIMEOUT_TICKS = 400` now remains a true 4.0 second wait instead of overflowing to 144 ticks.
