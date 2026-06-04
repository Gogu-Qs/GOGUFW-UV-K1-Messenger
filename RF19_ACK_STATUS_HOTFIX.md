RF19 ACK status hotfix
======================
Base: RF18 ACK + Retry.

Observed behavior:
- Receiver successfully sends ACK.
- Sender hears/receives ACK FSK burst.
- Sent list still changes from ? to x.

Fix:
- Treat any valid decoded ACK packet as authoritative for its MsgID.
- Update outbox status to ACKED even if the pending ACK state already timed out.
- If the ACK matches the active pending MsgID, also stop the retry timer.

Not changed:
- RF timing.
- Preamble.
- Boot-prime.
- Voice restore.
- Retry count.
