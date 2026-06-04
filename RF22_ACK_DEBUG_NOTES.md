# RF22 ACK Debug Build

Base: RF21 from RF19 (RF20 delayed-ACK changes are not included).

Purpose: isolate why ACK RF bursts are heard but Sent status does not change to `+`.

Changes:
- Keeps RF21 UI/branding changes.
- Replaces old RX/TX/register debug display with ACK-specific debug values.
- Does not change RF timing/preamble/voice path.

Messenger HOME debug line:
- `Pxxxx` = current pending message MsgID waiting for ACK
- `Axxxx` = last received ACK_FOR MsgID parsed by sender
- `R` = ACK packets parsed/received count
- `M` = ACK match count while waiting

Messenger SETTINGS debug line alternates:
- `Pxxxx Axxxx Rn Mn`
- `Sxxxx Wn Tn Xn`
  - `Sxxxx` = last ACK MsgID transmitted by receiver
  - `W` = sender wait-ACK active flag
  - `T` = retry count
  - `X` = ACK mismatch count

Test goal:
1. Send one message with DEBUG ON.
2. If ACK FSK is heard but `R` stays 0, ACK packet is not being parsed.
3. If `R` increases but `A` != `P`, ACK MsgID format/mapping is wrong.
4. If `R` and `M` increase but Sent still shows `?`/`x`, outbox status update path is wrong.
