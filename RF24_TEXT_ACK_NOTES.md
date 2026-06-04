# RF24 text ACK fallback

Base: RF23/RF21 UI line.

Changes:
- ACK is now transmitted as a normal GGFW TEXT packet with payload `ACK:hhhh`.
- The receiver does not store `ACK:hhhh` in Inbox; it treats it as ACK for the pending MsgID.
- This avoids the separate short ACK packet parser path that stayed `R0` even though ACK FSK audio was heard.
- RF timing, RF13 preamble, UI prev/next, Sent RESEND and GOGUFW branding are preserved from the previous line.
