# GOGUFW v1.1.2 batch fix

Applied requested fixes only:

1. Inbox Read screen footer label
   - Inbox/read message left soft label changed from `RE:` to `REPLY`.
   - Compose/reply prefix remains `RE: ` and was not changed.

2. Screen-off / SetOff RX wake compatibility
   - Removed the `gWakeUp ? 200 : 10` deep RF power-save multiplier.
   - Screen-off now keeps the normal RF power-save cadence (`BATTERY_SAVE * 10`) so Messenger / Range Check wake packets have a much better chance to be received.
   - On `sqlLost` while `gWakeUp` is active, the display/backlight is restored immediately and `gWakeUp` is cleared so RX can proceed normally.

Build was not run in this environment because the ARM embedded toolchain is not installed.
