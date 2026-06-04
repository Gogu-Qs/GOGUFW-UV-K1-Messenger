RF21 from RF19 base
====================

Base: RF19 ACK status hotfix, not RF20. RF20 delayed-ACK timing was avoided because it was reported to introduce RX/open-state regressions.

Changes:
- Robust ACK MsgID handling: ACK mirrors original MsgID in payload[0..1]; if one ACK is pending, any valid ACK frame marks that pending MsgID as ACKED and stops retry/timeout.
- Inbox/Sent READ screen: UP/LEFT = previous message, DOWN/RIGHT = next message. Draft edit screens are not affected.
- Sent READ MENU now RESENDs the same text instead of REPLY.
- Branding defaults changed to GOGUFW 5.5.1.

Preserved:
- RF19 RF timing/preamble/voice behavior.
- No RF20 delayed ACK changes.
- 36-character message limit.
