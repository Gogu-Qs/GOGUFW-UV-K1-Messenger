# PR #423 port notes for GOGUFW 1.0.3

Base: GOGUFW_1.0.3_PR405_checked.zip
Scope: armel/uv-k1-k5v3-firmware-custom PR #423 only.

Checked/applied commit by commit:

- 337eb0c Update donors: not applied intentionally; README donor text is upstream project metadata and not firmware behavior.
- e2e9c9b / 20b87fe Code refactoring (4 B): applied to App/ui/status.c.
- 887e18e Fix reset defaults: applied to App/settings.c.
- e6a727e Fix roger beep: applied to App/driver/bk4819.c, App/driver/bk4829.c, App/driver/bk4819.h, App/radio.c.
- 80e7d4b Fix F+4 frequency scan sensitivity: applied to App/app/scanner.c.
- c775fa8 Update donors: not applied intentionally; README donor text is upstream project metadata and not firmware behavior.
- 8723863 Fix F+4 verify VHF harmonic before accepting frequency: applied to App/app/scanner.c.
- bca06e3 Fix NAR+ FM demod boost: applied to App/driver/bk4829.c.
- dcbbd74 Add screenshot state flags: applied to App/driver/bk4819.c, App/driver/bk4829.c, App/driver/bk4819.h, App/screenshot.c.
- f069ed9 Fix screensaver state during deep sleep: applied to App/app/app.c.
- a293640 Fix screenshot state updates without display redraw: applied to App/app/app.c, App/screenshot.c, App/screenshot.h.
- 65a5c9f Prepare new version v5.6.1: upstream archive binaries were not added; GOGUFW version string remains 1.0.3 intentionally.

Build note:
- CMake preset Fusion was checked, but build/configure cannot proceed in this environment because arm-none-eabi-gcc and arm-none-eabi-g++ are not installed.
