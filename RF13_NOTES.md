# GGFW RF13 - RF12 + TX long preamble/lead-in

Base: RF12 global notify beep build.

Goal:
- Improve the observed "first 1-2 messages after idle may be missed, then reception becomes stable" behavior.
- Do not duplicate the message frame and do not affect future ACK/MsgID semantics.
- Do not touch RX state, REG_47, REG_59 receive enable behavior, or the RF12 boot-prime/beep logic.

Change:
- Messenger TX no longer calls the stock `BK4819_SendFSKData()` directly.
- It uses the same TX FIFO/send flow but sets REG_59 preamble length to max hardware value:
  - old Aircopy TX: `0x8068 / 0x0068 / 0x2868` = 7-byte preamble + 4-byte sync
  - RF13 Messenger TX: `0x80F8 / 0x00F8 / 0x28F8` = 16-byte preamble + 4-byte sync

Expected:
- Better first-message-after-idle reception.
- No duplicate inbox messages.
- Voice RX/TX behavior should remain like RF12.
- Global beep should remain like RF12.
