# GOGUFW 0.5.17 Repeat Stability Test Build

Base: GOGUFW 0.5.16 range timing / keepalive source.

Changes:
- Version bumped to 0.5.17.
- Range Check PING is transmitted as 3 identical frames with a short gap.
- Range Check PONG is transmitted as 2 identical frames with a short gap.
- Messenger ACK fallback frame is transmitted twice with the same ACK payload/id.
- Normal Messenger text messages are not duplicated; existing ACK/retry behavior is preserved.

Goal:
- Improve practical decode probability when the receiver hears FSK audio but misses the single frame sync/decode.
- Test whether Range Check and ACK reliability improves with repeated short frames without changing the main RF modem configuration.
