# RF23 ACK RX-window + FSK-aware TX scheduler

Base: RF22 ACK debug / RF21 UI branding line.

Changes:
- ACK reception fix attempt: after a manual message TX, immediately primes the sidecar FSK receiver for the ACK window instead of waiting for the normal re-arm delay. This targets the observed `R0` ACK debug case where ACK FSK is audible but not parsed.
- TX busy rule refined: Messenger TX is blocked only during active FSK capture (`s_rx_capture_active` / stale capture window), not by ordinary voice RX/carrier alone.
- ACK TX and retry TX now use the same FSK-aware rule; if FSK capture is active, they defer rather than being dropped.
- ACK packet path still uses the same long-preamble Messenger TX envelope.

Test focus:
- Sender RF23, receiver RF21/RF22/RF23 acceptable; for full two-way debug use RF23 on both.
- Send one message, then inspect ACK debug:
  - R should increase if ACK packet is parsed.
  - M should increase and Sent status should become `+` if ACK matches.
- Confirm ordinary voice RX does not prevent manual Messenger TX.
- Confirm voice RX/TX still works after message/ACK/retry.
