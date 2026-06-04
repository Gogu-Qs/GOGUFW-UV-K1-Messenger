# GGFW RF18 - Stage 2 ACK + 1 Retry

Base: RF17 stable Stage 1 baseline.

Added:
- ACK packet type support (`MSG_PKT_TYPE_ACK`).
- Receiver queues ACK after a valid text packet is decoded.
- Sender tracks one pending ACK at a time.
- If no ACK arrives after timeout, sender retries the same MsgID once.
- Outbox status markers:
  - `?` pending ACK
  - `+` ACK received
  - `x` failed after one retry

Preserved:
- RF17 boot-time RX prime and REG_3F OR-lock.
- RF13 16-byte TX preamble.
- Global RF message beep.
- 36-character message limit.
- Draft save+send.
- 6-row Inbox/Sent/Drafts lists.

Deliberately not included yet:
- Hop/relay forwarding.
- Multi-pending ACK queue.
- Unicast-only ACK rules.
- CCA random backoff UI.

Test focus:
1. First-message-missed pattern: sender should retry once and then get ACK.
2. Outbox should change from `?` to `+` when ACK arrives.
3. If receiver is off/out of range, outbox should become `x` after retry.
4. Voice RX/TX must remain normal after message + ACK traffic.
