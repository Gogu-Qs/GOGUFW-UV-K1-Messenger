# RF25 ACK text packet-id fix

Based on RF24 buildfix.

Change:
- ACK is still sent as normal text payload `ACK:hhhh`.
- The ACK frame now uses a fresh packet MsgID.
- The original message MsgID is carried only inside the ACK text payload.

Reason:
Using the original message MsgID as the ACK packet MsgID may be filtered/confused with an already seen outgoing message path. This build tests whether ACK text is then parsed and can update sent status to `+`.
