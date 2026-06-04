# GOGUFW v0.5.11 F+9 Call Tone test4

Changes from test3:

- `CllTon` and `CllVol` menu names use mixed-case labels.
- Menu order changed: `SetScn` -> `CllTon` -> `CllVol` -> `MsgRx`.
- `CllVol` now controls the RF call-tone modulation gain path, not the local speaker preview/monitor level.
- Local preview/sidetone is forced very low and independent from `CllVol`.
- `CllTon` menu preview now plays the full call-tone duration (~3.5 seconds), matching F+9 transmit duration.
- Main screen TX label during F+9 call tone changed from `CALL TX` to `CALLTX` to avoid overlap with channel name.
- Normal PTT TX label remains `TX`; long-press 9 still keeps the original 1 CALL channel behavior.

Test focus:

1. Verify `CllVol` changes receiver-side loudness: LOW / MID / HIGH.
2. Verify local speaker preview remains quiet at every volume level.
3. Verify `CllTon` preview is full length.
4. Verify normal PTT TX still displays `TX`, while F+9 displays `CALLTX`.
