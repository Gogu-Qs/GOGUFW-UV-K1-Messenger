## GOGUFW 1.3.0

GOGUFW 1.3.0 expands Spectrum operation while preserving the established Messenger/FSK packet format and the normal F4HWN radio experience.

### Memory Spectrum

- Added a dedicated Memory Spectrum mode that scans valid stored channels when Spectrum is opened from MR mode.
- Displays the current channel name prominently together with its frequency and memory range.
- Uses each stored channel's modulation and bandwidth when listening to a peak, including AM airband channels.
- Memory Spectrum remains manual-threshold only for predictable operation across mixed channel noise floors.
- Uses a compact valid-channel bitmap and reads channel details only when needed, avoiding a large RAM channel table.

### Spectrum peak selection

- The first PTT opens and listens to the detected peak.
- A second PTT opens the selected stored channel in MR mode.
- In VFO Spectrum, the second PTT transfers the selected frequency, modulation, bandwidth and nearest step to the active VFO.
- Peak selection is handled after PTT release, so it changes channel/frequency without briefly transmitting or producing a TX burst.
- Fixed power-on reopening Spectrum after a peak had been assigned; the radio now returns to and starts on the main screen.
- Prevented the graph from collapsing during sweep restarts and retained the last valid peak frequency instead of briefly displaying `0.0000`.

### Spectrum quick start

- Press **F + 5** from MR mode for Memory Spectrum or from VFO mode for VFO Spectrum.
- Press **PTT once** to listen to a peak.
- Press and release **PTT a second time** to open the stored MR channel or transfer the peak to the active VFO without transmitting.
- Use **UP/DOWN** to resume scanning and **EXIT** to step back from the peak view or leave Spectrum.
- See `SPECTRUM.md` in the repository for the complete key reference.

### Messenger and Range Check

- Range Check now displays PONG results as they arrive while keeping the complete 12-second collection window active for other responders.
- Added a short Messenger-only FSK sync hold so Dual Watch remains on the correct receive VFO across closely timed wake/message frames.
- Kept longer ACK and Range Check channel locks intact when additional FSK sync events arrive.
- Preserved the existing GOGUFW message, ACK, retry, ping and pong packet formats.

### Radio and interface refinements

- Added clearer scan-list feedback on the main screen when the active list changes.
- Improved targeted channel-attribute updates when channels or scan-list membership change.
- Kept UART/USB command servicing responsive while supported full-screen applications are active.
- Updated firmware and CHIRP identification to version 1.3.0.

### Included files

- `GOGUFW-1.3.0-Fusion.bin` — Fusion firmware for supported Quansheng UV-K1 / UV-K5 V3 hardware using the PY32F071 MCU and BK4829 RF IC.
- `Gogufw_1.3.0_chirp_module.py` — matching custom CHIRP module, listed under the Quansheng vendor.

Back up the radio, calibration data and CHIRP configuration before flashing. This experimental third-party firmware is provided without warranty.
