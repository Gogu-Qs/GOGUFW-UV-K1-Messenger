# GGFW RF10 - REG_3F FSK IRQ OR-lock targeted fix

Base: RF9 state-diff probe.

Observed by user:
- No message decode state: `3F:0C0C`, `59:3068`
- Message decode/open-RX state: `3F:3002`, `59:3068`
- Therefore REG_59 FSK RX enable was not the differentiator; FSK IRQ enable bits in REG_3F were.

Targeted change:
- Keep `REG_59` handling from RF9/RF8.
- Do not touch `REG_47` for FSK RX.
- Add `MSG_RF_EnsureFskIrqMask()` which reads current `REG_3F` and writes `current | 0x3002` only if needed.
- Call this while idle / not transmitting so FSK SYNC, FIFO almost full, and RX finished interrupts remain enabled before the next burst.
- Preserve existing F4HWN interrupt bits; do not replace REG_3F with a fixed value.

Test focus:
1. Does `3F` stay with FSK bits enabled instead of falling to `0C0C`?
2. Do messages arrive without waiting for SQL-open/stuck RX?
3. Does normal voice RX/TX remain clean?
4. Does the 5-6 message RX-open/hiss state still occur?
