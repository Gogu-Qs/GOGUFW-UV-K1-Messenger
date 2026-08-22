# GOGUFW 1.0.3 - PR #405 audit / port notes

Source request: apply **armel/uv-k1-k5v3-firmware-custom PR #405 Feature update v5** on top of the uploaded GOGUFW 1.0.3 source.

## Result

No additional source-code patch was required during this pass. The uploaded GOGUFW 1.0.3 tree already contains the important PR #405 / F4HWN 5.6.0 changes that matter for this project.

## Checked PR #405 groups

- Boot beep with `POnMsg LOGO`: already present in `App/driver/backlight.c`.
- `SetSav` / BLMin screen saver infrastructure: already present in `App/app/app.c`, `App/settings.*`, `App/ui/menu.*`, `App/ui/welcome.c`.
- `LOGO+` scrolling saver mode: already present.
- Saver guards: TX/RX/Beam/FM scan/M-SCAN related blocks are already represented in the app flow.
- F+4 frequency-copy scan UI improvements: already present in scanner/UI areas.
- Scan RSSI sparkline indicator: already present in `App/app/chFrScanner.c` and `App/ui/main.c` paths.
- VFO lock icon scan placement refinement: already present in UI main/status logic.
- Scan-range subaudible CTCSS/DCS detection: already present in `App/app/chFrScanner.*` and `App/ui/main.c`.
- AM to FM dual-watch RX reconfiguration fix: already present in `App/radio.c` with GOGUFW 0.6.6 / F4HWN 5.6.0 note.
- Screenshot RAM/stack reductions: screenshot code path already includes the newer chunk-based implementation guards.
- Backlight PWM SRAM reduction and hollow manual bulb icon: already present in backlight/UI logic.
- Prepare v5.6.0/version-side upstream merge: already represented by the earlier GOGUFW 0.6.6 real 5.6.0 merge notes; local project version intentionally remains `1.0.3`.

## Messenger safety note

Messenger/HEARD/Range Check specific files were not rewritten in this pass. The PR #405 feature set was found already integrated around the F4HWN/UI/radio/backlight/scanner areas, so there was no need to reapply broad upstream patches that could overwrite GOGUFW Messenger logic.

## Build note

A local configure attempt was made with `cmake --preset Fusion`, but this container does not have `arm-none-eabi-gcc` / `arm-none-eabi-g++` installed, so a compile test could not be completed here.
