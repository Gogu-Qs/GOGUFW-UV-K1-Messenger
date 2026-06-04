# GGFW RF27 controlled re-prime test

Base: RF26 ACK-visible + numeric T9.

Changes:
- Adds controlled global FSK RX re-prime after TX/RX, delayed by ~300 ms.
- Adds periodic idle re-prime every ~8 seconds, only when no RX/carrier and no TX.
- ACK TX is delayed ~800 ms so the sender has time to re-prime after its own TX.
- ACK timeout is ~2.5 seconds.
- Keeps ACK visible in Inbox for debugging.
- Keeps numeric T9 mode: B -> b -> 2 with *.
- Does not touch REG_47 / voice AF path.
- Re-prime is blocked during voice RX, FSK capture, PTT/TX, or active carrier.

Test focus:
1. Does normal voice RX remain free of ticks/cuts?
2. Does scan/idle/TX-after recovery improve?
3. Does ACK text arrive in Inbox and/or does Sent status become +?
4. Do normal messages approach 10/10 after idle?
