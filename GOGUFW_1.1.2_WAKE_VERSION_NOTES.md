# GOGUFW 1.1.2 wake preamble + version update

Applied on top of `GOGUFW_1.0.3_PR405_PR423_FSKVOXFM`.

Changes made:

- Added invisible Messenger WAKE packet type (`MSG_PKT_TYPE_WAKE = 5`).
- Added `MSG_PACKET_BuildWake()` helper.
- RX parser now accepts WAKE frames but handles them silently:
  - no Inbox entry
  - no HEARD entry
  - no beep
  - no ACK
  - only finishes the current FSK RX attempt so the following real text packet can be received more reliably after Power Save / Dual Watch wake-up.
- `MSG_RF_SendText()` now sends one WAKE frame before the first real TEXT frame only.
- Retry path was not changed; retry frames remain plain TEXT and do not send WAKE first.
- Visible/build version strings updated to `v1.1.2`:
  - `CMakeLists.txt` default `VERSION_STRING_2`
  - `CMakePresets.json` `VERSION_STRING_2`
  - welcome screen text `GOGUFW 1.1.2`

Build note:

- Build was not run in this environment because `arm-none-eabi-gcc` is not installed.
