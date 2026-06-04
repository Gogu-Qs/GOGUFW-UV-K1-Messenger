# GOGUFW 0.5.18 Range PONG Timing Hotfix

Base: GOGUFW 0.5.17 repeat stability test build.

## Changes

- Version bumped to 0.5.18.
- Range PING repeat behavior is unchanged: 3 identical PING frames.
- Range PONG repeat behavior is unchanged: 2 identical PONG frames.
- Messenger ACK repeat behavior is unchanged: 2 identical ACK frames.
- Range PONG response timing changed from approximately 0.5–6.0 seconds to approximately 3.0–6.0 seconds.

## Reason

With 3 repeated PING frames, a responder could schedule a PONG too early and collide with the transmitter still sending PING repeats. The new 3–6 second PONG window keeps replies inside the 10 second listen window while avoiding overlap with the repeated PING burst.
