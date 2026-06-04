# GGFW RF15 48-char Drafts Build

Base: RF15 64-char drafts build, adjusted to avoid RAM overflow without reducing inbox/outbox/draft capacities.

Changes:
- Shared message limit changed from 64 to 48 characters via `MSG_TEXT_LEN`.
- Compose, Reply, Draft edit/create and packet payload use the same 48-character limit.
- Inbox/outbox/draft capacities remain at RF15 v1 values: Inbox 20, Outbox 10, Drafts 8.
- RF13/RF12 radio behavior is otherwise preserved.

Test focus:
1. Fusion build RAM usage should fit.
2. RF message success should remain like RF13/RF12.
3. 48-character messages should transmit and receive complete.
4. Draft create/edit should save and persist after power cycle.
