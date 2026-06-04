# RF15 36-char final RAM-safe baseline

Base: RF13/RF12 stable RF messaging line.

Changes:
- Common message text limit set to 36 characters via MSG_TEXT_LEN.
- Compose, Reply, Draft edit/create, packet payload, inbox/outbox/draft storage all share MSG_TEXT_LEN.
- Inbox/Outbox/Draft capacities preserved for future ACK/hop work.
- RF13 16-byte preamble retained.
- RF14 warm-up frame is not included.
- Draft save/update persistence fix retained.

Reason:
- 64 chars caused RAM overflow.
- 48 chars may build but leaves less margin for ACK/retry/hop/seen-cache.
- 36 chars preserves RAM headroom for next protocol stages.
