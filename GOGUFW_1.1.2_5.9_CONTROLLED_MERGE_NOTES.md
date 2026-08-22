# GOGUFW 1.1.2 -> 5.9 controlled merge candidate

This is a source-only first candidate based on the supplied GOGUFW 1.1.2 tree.
No version bump has been applied yet.

## User-reported bug fixes

1. FM radio periodic tick / redraw
   - FM RSSI is now cached.
   - The FM screen no longer reads BK1080 RSSI directly on every redraw.
   - The 500 ms task samples RSSI and requests an FM redraw only when the 0..5 bar level changes.
   - This removes the unconditional 500 ms FM redraw that was introduced to keep RSSI live.

2. CHIRP programmable key action loss
   - Added MESSENGER, HEARD and CALLTX after BEAM in KEYACTIONS_LIST.
   - Their indexes now match firmware ACTION_OPT_t.
   - CHIRP no longer converts those valid actions to NONE during read -> write.

3. Analog RX / squelch state occasionally sticking
   - Messenger FSK soft mute still saves/restores BK4829 REG_48.
   - It no longer snapshots/restores gEnableSpeaker.
   - FSK mute never changed gEnableSpeaker, so restoring a stale copy could overwrite the stock squelch close state.

## Selected low-risk upstream 5.9 changes

- py25q16: erase a NOR sector only when a requested write needs a 0 -> 1 bit transition.
- eeprom_compat: extend 0x009000 VFO mapping through 0x0090E7, fixing the old truncated final VFO record.
- chFrScanner: scan range skip/exclusion slots 32 -> 64.
- scanner: clear VHF second-harmonic verification state when stopping.
- action/app: stop channel/frequency scan before entering FM and do not let main-channel RX interrupt an active FM scan.
- radio: remove duplicate modulation programming while retaining the single required retune modulation update.

## Intentionally not merged yet

The following 5.9 features are larger optional subsystems and are not part of this first bugfix candidate:
- Fox Hunt
- RX/TX Log
- Action Picker
- K5Viewer migration
- Menu Categories
- New upstream LOGO/LOGO+ screen-saver restructuring

These need separate flash/RAM and Messenger/FSK regression review.

## Validation performed

- CHIRP Python module passes `python3 -m py_compile`.
- Source diff reviewed against supplied F4HWN 5.9 tree.
- ARM firmware binary was not built in this environment because arm-none-eabi-gcc is not installed.
