# GGFW RF6 known-good merge

Basis: clean Stage 6-7 native packet/UI baseline.

Intent: stop the trial-and-error chain and merge only the lessons that were actually useful:

- 8G-style voice/mic restore before PTT and after Messenger TX.
- No Messenger UI enter/exit RF init/deinit.
- No RX-start register writes.
- No idle-arm loop that repeatedly disrupts voice RX.
- Never select REG_47 FSK test audio; normal AF output must be preserved.
- FSK RX sidecar is armed only when idle, using exact Aircopy RX register values, and only after a voice snapshot has been captured.
- FSK interrupt handling reads FIFO only on FSK sync/FIFO/RX-finished events.

Test priorities:
1. Normal voice RX/TX before Messenger.
2. Messenger enter/exit then normal voice RX/TX.
3. Compose->MENU then normal voice TX mic audio.
4. RF TX/RX activity.
5. Real message receive count and debug counters T/S/F/D/R.
