# GGFW RF11 boot-prime + beep fix

Base: RF10 REG_3F OR-lock.

Targeted changes:
- Messenger config is now loaded once from the global RF tick, not only when the Messenger UI is opened.
- This lets msg_rx/msg_beep be valid from boot, so the FSK sidecar can be primed before the first real packet.
- One-time boot/global RX prime uses the existing idle-only `MSG_RF_ArmSidecarIfIdle()` path.
- REG_47 is not touched for RX.
- REG_59 behavior is not changed from RF10/RF8 v2; FSK RX remains enabled when armed.
- REG_3F FSK IRQ OR-lock remains active.
- RF message beep is deferred and has a fallback timeout so it is not lost forever if RX busy timing blocks the initial beep window.

Primary test expectations:
1. Receiver should be able to receive without opening Messenger first.
2. First packet should be less likely to be sacrificed as a priming packet.
3. Voice TX/RX should remain normal before/after Messenger and after message TX.
4. Real RF received-message beep should be more likely to play after RX idle.
