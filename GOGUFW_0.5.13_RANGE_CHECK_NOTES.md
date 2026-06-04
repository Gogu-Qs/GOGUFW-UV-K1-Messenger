# GOGUFW 0.5.13 Range Check update

Changes from 0.5.12 hotfix3:

- Version bumped to 0.5.13.
- Range PING/PONG remains on the Messenger native FSK packet path instead of adding a separate RF path.
- Incoming Range PING now queues automatic Range PONG without creating inbox/unread state.
- Range events use a single short beep path, separated from the normal message double-beep.
- Range PONG carries remote battery voltage in centivolts; older/empty packets display `--.-`.
- Range result rows keep one-device-per-line layout: callsign, RSSI, battery, and signal bars on the same row.
- After sending a Range PING, RX is temporarily locked to the sending VFO for the full 10 second PONG wait window.
- Messenger ACK wait also uses the same temporary RX VFO lock to reduce ACK misses caused by Dual Watch alternation.
- Temporary RX lock is exposed through the existing FM-radio-style HOLD/lock status indication.
- Boot/welcome version inverted capsule is sized from the actual version string to avoid final digits appearing outside the inverted area.

Validation performed in this environment:

- Docker firmware build could not be run because Docker is not installed in the sandbox.
- Host GCC syntax-only checks were run on the modified Messenger/Range/UI files and passed; embedded pointer-size warnings are expected on host GCC.
