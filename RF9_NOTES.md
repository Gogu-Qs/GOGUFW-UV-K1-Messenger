# GGFW RF9 state-diff diagnostic

Base: RF8/RF7 line. This package intentionally does **not** add a new RX strategy.

Goal: identify which BK4829 state changes when messages start decoding only after the radio falls into the unwanted open-RX / SQL0-like condition.

What changed vs RF8:
- Added REG_67 RSSI snapshot.
- Messenger Settings debug bottom line now rotates every ~0.6s:
  - `0Cxxxx 59xxxx` squelch/link + FSK RX control
  - `02xxxx 0Bxxxx` interrupt/status + FSK sync latch
  - `30xxxx 3Fxxxx` RX/DSP/link enables + IRQ mask
  - `47xxxx 67xxxx` AF output selection + RSSI
- RF logic is otherwise kept conservative. REG_59 holding behavior from RF8 remains so we can compare against the previous test.

Test notes to record:
1. Before messages decode: all four debug pages.
2. While FSK is heard but no inbox decode: all four debug pages.
3. When the radio falls into open-RX / SQL0-like state and messages decode: all four debug pages.
4. After successful decode: whether voice TX/RX remains OK.

Key question:
If REG_59 remains `3068`, what changes in REG_30/0C/02/0B/3F/67 when decode starts working?
