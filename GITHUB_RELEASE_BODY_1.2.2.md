# GOGUFW 1.2.2

GOGUFW 1.2.2 is a reliability and interface maintenance release for Quansheng UV-K1 / UV-K5 V3 radios using the PY32F071 MCU and BK4829 RF IC.

## Changes

- Improves first-message reception during normal Battery Save wake cycles by treating an explicit FSK sync as a fresh packet boundary and discarding stale partial wake-frame data.
- Preserves normal Battery Save operation; Messenger does not force the receiver to remain continuously awake.
- Keeps Messenger FSK packet audio muted until the data carrier has ended, without taking ownership of normal analog squelch or speaker state.
- Gives normal RX/TX LEDs priority over the unread-message notification and resumes the notification after radio activity ends.
- Adds responsive local previews for the Roger and MDC menu choices without transmitting RF.
- Right-aligns Inbox and Sent age indicators.
- Moves FM memory names one pixel lower inside their panel.
- Includes the matching GOGUFW 1.2.2 CHIRP module.

## Compatibility

- Messenger framing, message IDs, ACK/retry format, PING/PONG format and duplicate detection remain compatible with earlier GOGUFW 1.x radios.
- Intended only for supported UV-K1 / UV-K5 V3 hardware with PY32F071 + BK4829.

## Files

- `GOGUFW-1.2.2-Fusion.bin`
- `Gogufw_1.2.2_chirp_module.py`
