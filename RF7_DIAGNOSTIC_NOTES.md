# GGFW RF7 diagnostic controlled build

Base: RF6 known-good merge.

Goal: identify exactly which BK4829 state makes messages decode when the radio falls into the unintended open-RX/no-squelch condition, without adding new aggressive RX logic.

Safety rules preserved:
- Do not change REG_47 for FSK RX.
- No RX-start register writes.
- No periodic re-listen loop added.
- RF6 TX/voice restore path preserved.

Messenger HOME debug line when DEBUG=ON:
- T = RF TX count
- S = FSK sync events
- F = FIFO events
- D = decoded/inbox packets
- O = consecutive 10ms ticks with REG_0C squelch/link bit set

Test focus:
1. Before open-RX appears, record T/S/F/D/O after messages fail.
2. When open-RX/hiss appears, record T/S/F/D/O.
3. When messages decode, record whether O is high and whether S/F/D climb.

This build is intentionally diagnostic-first. The next build should copy only the state that enables S/F/D while preventing speaker hiss and stale RSSI.

Messenger SETTINGS bottom line when DEBUG=ON:
- 0C = REG_0C snapshot, especially bit1 squelch/link and bit0 IRQ
- 59 = REG_59 snapshot, especially FSK RX enable/clear/scramble state

If open-RX appears, go to Messenger SETTINGS and record the bottom line.
