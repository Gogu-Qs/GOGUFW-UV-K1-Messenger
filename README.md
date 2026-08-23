# GOGUFW 1.3.0 — Multiboot Adaptation

GOGUFW is a custom firmware for the Quansheng UV-K1 / UV-K5 V3, built on the F4HWN Fusion firmware and focused on radio-to-radio messaging and practical everyday tools.

This branch is an **experimental multiboot guest adaptation**. It makes GOGUFW compatible with the F4HWN multiboot slot layout while preserving the Messenger, HEARD, Range Check, CALLTX and FM extensions developed for GOGUFW.

The stable release remains [GOGUFW 1.2.2](https://github.com/Gogu-Qs/GOGUFW-UV-K1-Messenger/releases/tag/v1.2.2). The work on this branch does not modify the `main` branch or the stable release.

## About this multiboot work

The goal is to make GOGUFW a safe **guest firmware**, not to create another multiboot manager.

This adaptation adds:

- compatibility with the standard F4HWN multiboot firmware slots and per-slot settings profiles;
- a boot-time selector, opened by holding **MENU while powering on**;
- automatic one-time backup of a normally flashed firmware as the **Main** slot;
- CRC validation of a complete slot image before internal Flash is erased;
- a RAM-resident restore routine that continues working while internal Flash is unavailable;
- redundant active-profile records, so an interrupted marker update does not silently select the wrong settings bank;
- independent GOGUFW Messenger settings/drafts and FM station names for every user slot.

GOGUFW does **not** include the host-side commands used to upload, erase or manage firmware slots. Slots must be prepared with a compatible F4HWN multiboot manager/tool. GOGUFW only identifies the active profile, displays valid slots and safely restores the selected image.

### Preventing cross-firmware conflicts

Each slot receives its own radio configuration area. Normal channel memories, VFO settings and menu settings therefore do not overwrite the configuration used by another firmware slot.

GOGUFW also keeps its custom persistent data separate:

| Data | Multiboot behavior |
| --- | --- |
| Channels, VFOs and radio settings | Stored in the active slot's private profile bank. |
| Messenger configuration and drafts | Stored in GOGUFW-reserved space inside the active profile bank. |
| FM station names | Stored in a separate GOGUFW-reserved sector inside the active profile bank. |
| Main profile data | Keeps the historical GOGUFW addresses, so an existing installation does not require migration. |
| Calibration and stock voice data | Remain shared and are outside the slot/profile allocation. |

The reserved GOGUFW areas are currently beyond the standard F4HWN 5.9.0 settings footprint. Compile-time boundary checks prevent the build if the known slot, profile, marker or stock voice regions overlap.

No changes were made to Messenger packet framing, ACK/retry logic, the FSK wake preamble, BK4829 RF setup, squelch handling or Battery Save behavior as part of this adaptation.

## First boot and slot selection

Before testing an experimental build, save a complete CHIRP backup of the radio.

1. Flash the Fusion binary normally, or install it into a slot using a compatible multiboot manager.
2. A normal standalone installation shows **Init Main / DO NOT POWER OFF** once while the running firmware is backed up as Main. Do not switch the radio off during this operation.
3. For normal use, power on without holding a key.
4. To open the firmware selector, hold **MENU** while powering on.
5. Select a valid slot and confirm with **MENU**. Press **EXIT** to return without switching.

Only slots with a valid header, size and full-image CRC are offered for restore. A successful switch rewrites the internal application region and resets the radio.

### Current build status

The Fusion preset builds successfully as **GOGUFW 1.3.0**:

- RAM: **13,496 / 16,384 bytes (82.37%)**;
- actual FLASH image: **118,860 / 120,832 bytes (98.37%)**;
- remaining FLASH: **1,972 bytes**;
- restore stub: **920 bytes**, with all **85 branches** verified to remain inside its RAM section.

The multiboot adaptation adds approximately **5,740 bytes of FLASH** and **16 bytes of RAM** compared with the previous Fusion build. Because the firmware is close to the internal Flash limit, future features must continue to be reviewed carefully.

Recommended radio regression tests include first-message reception after a cold boot and long idle, ACK/retry, Range Check ping/pong, normal RX and squelch, voice TX, Battery Save wake cycles, FM audio/names, and switching away from GOGUFW and back without settings crossing between slots.

## What GOGUFW adds

- **Messenger:** compose and receive text messages directly on the radio, with Inbox, Sent, Drafts, Reply, Resend and delivery acknowledgements.
- **HEARD:** view recently heard Messenger stations together with callsign, signal level, packet type and age.
- **Range Check:** send a PING and receive the other radio's callsign, signal level and battery voltage.
- **CALLTX:** transmit one of five selectable call melodies, with volume selection and tone preview in the menu.
- **FM radio tools:** save station names, rename or delete memories, and follow the live FM signal-strength meter.
- **Custom shortcuts:** open Messenger and HEARD quickly or transmit CALLTX from programmable side keys.
- **GOGUFW CHIRP module:** configure supported radio settings, custom key actions and FM station names from CHIRP.

The normal F4HWN Fusion radio features remain available alongside these additions.

## Shortcuts

| Function | Shortcut | What it does |
| --- | --- | --- |
| Messenger | **F + MENU** | Opens Messenger directly from the main radio screen. |
| HEARD / Range Check | **F + 7** | Opens the HEARD screen. Press **MENU** there to start a Range Check PING. |
| CALLTX | **F + 9** | Transmits the selected call melody. |
| Messenger | Assign **MESSENGER** to a programmable side-key action | Opens Messenger. Pressing the same assigned key on the Messenger home screen closes it. |
| HEARD | Assign **HEARD** to a programmable side-key action | Opens HEARD / Range Check. Pressing the same assigned key again closes it. |
| CALLTX | Assign **CALLTX** to a programmable side-key action | Transmits the selected call melody directly. |

The programmable actions can be assigned to the short or long press of the side keys from the radio menu or the included CHIRP module.

## Screens

### Messenger

| Messenger home | Compose a message |
| --- | --- |
| ![Messenger home screen](uv-k5-screenshot17.png) | ![Messenger compose screen](uv-k5-screenshot19.png) |

### HEARD and Range Check

| Recently heard stations | Range Check result |
| --- | --- |
| ![HEARD station list](heard.png) | ![Range Check result](rangecheck.png) |

### FM radio

| Live FM signal meter | Named FM station memory |
| --- | --- |
| ![FM radio signal meter](radio_vfo.png) | ![Named FM station memory](radio_name.png) |

### Programmable key actions

![Selecting a GOGUFW side-key action](f1short.png)

## Download and compatibility

The current stable release is **GOGUFW 1.2.2**. Its release page includes:

- the Fusion firmware `.bin` file;
- the matching `Gogufw_1.2.2_chirp_module.py` CHIRP module.

GOGUFW is intended for Quansheng UV-K1 / UV-K5 V3 variants using the **PY32F071 MCU and BK4829 RF IC**. It is not intended for unrelated BK4819-based radios.

[Open the GOGUFW 1.2.2 release](https://github.com/Gogu-Qs/GOGUFW-UV-K1-Messenger/releases/tag/v1.2.2)

## Build from source

The Fusion preset requires CMake, Ninja and the ARM GNU Embedded toolchain (`arm-none-eabi-gcc`):

```bash
cmake --preset Fusion
cmake --build --preset Fusion
```

For setup instructions, see [BUILD_VSCODE_MAC.md](BUILD_VSCODE_MAC.md) or [BUILD_WITH_VSCODE.md](BUILD_WITH_VSCODE.md).

## Credits

GOGUFW is based on the F4HWN / UV-K5 custom firmware project and retains the original project attribution and license. Thanks to the F4HWN contributors for the firmware foundation on which these additions were built.

## Disclaimer

This firmware is provided as-is. Users are responsible for complying with the radio regulations, licensing requirements and permitted frequencies applicable in their jurisdiction.
