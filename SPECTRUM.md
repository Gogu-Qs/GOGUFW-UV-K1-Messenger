# GOGUFW 1.3.0 Spectrum guide

GOGUFW provides two Spectrum modes selected automatically from the active radio mode.

## Opening Spectrum

- From **MR mode**, press **F + 5** to open Memory Spectrum. It scans the valid channels stored in the radio.
- From **VFO mode**, press **F + 5** to open VFO Spectrum. It scans a continuous frequency range around the active VFO.

## Selecting a peak

1. Wait for the graph to identify a peak.
2. Press **PTT once** to stop the sweep and listen to it.
3. If required, press **MENU** in the peak view to move through the LNA/PGA controls; use **UP/DOWN** to adjust the selected control.
4. Press and release **PTT a second time**.

In Memory Spectrum the radio opens the selected stored channel. In VFO Spectrum the detected frequency, modulation, bandwidth and nearest frequency step are transferred to the active VFO. The selection happens after PTT release and does not start a transmission.

After selecting a peak, the radio returns to the main screen. A later power-on also starts on the main screen rather than reopening Spectrum.

## Main controls

| Key | Graph view | Peak/listening view |
| --- | --- | --- |
| **PTT** | Listen to the current peak | On the next press and release, open the MR channel or transfer the VFO frequency |
| **UP / DOWN** | Move or restart the sweep in that direction | Return to scanning in that direction |
| **EXIT** | Leave Spectrum | Return to the Spectrum graph |
| **MENU** | VFO mode: switch manual/automatic threshold | Cycle LNA/PGA controls |
| **SIDE1** | Temporarily block the current peak | Toggle monitor mode |
| **SIDE2** | Toggle the backlight | Toggle the backlight |
| **3 / 9** | Adjust graph height or automatic sensitivity | — |
| **STAR / F** | Adjust the RSSI trigger level | — |
| **6** | Change listening bandwidth | Change listening bandwidth |
| **0** | VFO mode: change modulation | — |
| **1 / 7** | VFO mode: change scan step | — |
| **2 / 8** | VFO mode: change frequency movement step | — |

## Memory Spectrum notes

- Only valid stored MR channels are included.
- The channel name is the primary label; its frequency is shown underneath.
- The selected channel's stored modulation and bandwidth are used while listening, including AM airband memories.
- Memory Spectrum uses manual threshold control. This avoids automatic threshold changes becoming unstable when AM and FM memories have different background noise levels.
- Temporary blocked peaks are cleared when Spectrum is closed; they are not written as permanent channel exclusions.

## VFO Spectrum notes

- VFO Spectrum keeps the established continuous-frequency scanning behavior.
- Transferring a peak updates the active VFO with the detected frequency and reception settings.
- The last valid peak remains displayed while the next sweep begins, avoiding a temporary `0.0000` frequency display.
