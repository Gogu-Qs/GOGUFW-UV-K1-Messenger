# GGFW RF8 targeted fix

Base: RF7 state-probe.

RF7 field result showed:
- Messages not decoding: S increased but F/D stayed 0.
- In the accidental open-RX state, messages decoded and REG_59 was observed as 0x3068.
- Later REG_59 fell to 0x0068 and decode stopped.

RF8 changes:
- Preserve REG_59<12> FSK RX enable after successful or failed capture cleanup.
- Cleanup pulses RX FIFO clear then immediately restores REG_59 to 0x3068.
- REG_47 is not changed for RX.
- Voice restore / Messenger enter-exit behavior from RF7 is preserved.

Test focus:
1. Does REG_59 remain 3068 instead of falling to 0068?
2. Does boot/main-screen reception improve?
3. Does the 5-6 message SQL-open/hiss state still happen?
4. Does normal voice TX/RX remain OK before/after Messenger and after message TX?
